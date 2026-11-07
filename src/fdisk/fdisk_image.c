#include "fdisk_image.h"

#include "fdisk_error.h"

#include <stdlib.h>
#include <wchar.h>
#include <virtdisk.h>

static const GUID MICROSOFT_VIRTUAL_DISK_VENDOR = {
    0xec984aec, 0xa0f9, 0x47e9, {0x90, 0x1f, 0x71, 0x41, 0x5a, 0x66, 0x34, 0x5b}
};

static bool GetImageStorageType(const wchar_t* path, VIRTUAL_STORAGE_TYPE* type,
    FdiskError* error) {
    if (path == NULL || path[0] == L'\0') {
        FdiskErrorSet(error, ERROR_INVALID_NAME, L"Virtual disk image path is empty");
        return false;
    }
    const wchar_t* extension = wcsrchr(path, L'.');
    const wchar_t* backslash = wcsrchr(path, L'\\');
    const wchar_t* slash = wcsrchr(path, L'/');
    if (backslash != NULL && (slash == NULL || backslash > slash)) slash = backslash;
    if (extension == NULL || (slash != NULL && extension < slash)) {
        FdiskErrorSet(error, ERROR_INVALID_NAME, L"Virtual disk image paths must end in .vhd or .vhdx");
        return false;
    }

    type->VendorId = MICROSOFT_VIRTUAL_DISK_VENDOR;
    if (_wcsicmp(extension, L".vhd") == 0) {
        type->DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHD;
    } else if (_wcsicmp(extension, L".vhdx") == 0) {
        type->DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHDX;
    } else {
        FdiskErrorSet(error, ERROR_INVALID_NAME, L"Virtual disk image paths must end in .vhd or .vhdx");
        return false;
    }
    return true;
}

static void AppendApiError(FdiskError* error, const wchar_t* operation, DWORD code) {
    FdiskError secondary = {0};
    wchar_t primary[FDISK_ERROR_CAPACITY];
    (void)wcscpy_s(primary, _countof(primary), error->message);
    FdiskErrorSetWindows(&secondary, operation, code);
    (void)swprintf_s(error->message, _countof(error->message), L"%ls; additionally, %ls",
        primary, secondary.message);
}

static bool CloseHandleAfterError(HANDLE handle, FdiskError* error) {
    if (handle == NULL || handle == INVALID_HANDLE_VALUE) return true;
    if (CloseHandle(handle)) return true;
    AppendApiError(error, L"Could not close the virtual disk handle", GetLastError());
    return false;
}

static bool OpenImage(const wchar_t* path, DWORD access, HANDLE* handle, FdiskError* error) {
    VIRTUAL_STORAGE_TYPE storage_type = {0};
    OPEN_VIRTUAL_DISK_PARAMETERS parameters = {0};
    if (!GetImageStorageType(path, &storage_type, error)) return false;
    parameters.Version = OPEN_VIRTUAL_DISK_VERSION_1;
    parameters.Version1.RWDepth = 1;
    const DWORD status = OpenVirtualDisk(&storage_type, path, access,
        OPEN_VIRTUAL_DISK_FLAG_NONE, &parameters, handle);
    if (status != ERROR_SUCCESS) {
        FdiskErrorSetWindows(error, L"Could not open virtual disk image", status);
        return false;
    }
    return true;
}

static bool ParsePhysicalDrivePath(const wchar_t* path, uint32_t* disk_number) {
    static const wchar_t prefix[] = L"\\\\.\\PhysicalDrive";
    if (_wcsnicmp(path, prefix, _countof(prefix) - 1) != 0) return false;
    const wchar_t* cursor = path + _countof(prefix) - 1;
    if (*cursor == L'\0') return false;
    uint64_t number = 0;
    for (; *cursor != L'\0'; ++cursor) {
        if (*cursor < L'0' || *cursor > L'9') return false;
        const uint64_t digit = (uint64_t)(*cursor - L'0');
        if (number > (UINT32_MAX - digit) / 10u) return false;
        number = number * 10u + digit;
    }
    *disk_number = (uint32_t)number;
    return true;
}

static bool GetImageDiskNumber(HANDLE handle, uint32_t* disk_number, FdiskError* error) {
    ULONG path_size = 0;
    DWORD status = GetVirtualDiskPhysicalPath(handle, &path_size, NULL);
    if (status != ERROR_INSUFFICIENT_BUFFER || path_size < sizeof(wchar_t) ||
        path_size % sizeof(wchar_t) != 0) {
        FdiskErrorSetWindows(error, L"Could not determine the attached image's physical disk path",
            status == ERROR_SUCCESS ? ERROR_INVALID_DATA : status);
        return false;
    }
    wchar_t* path = (wchar_t*)malloc(path_size);
    if (path == NULL) {
        FdiskErrorSet(error, ERROR_NOT_ENOUGH_MEMORY, L"Could not allocate the virtual disk path buffer");
        return false;
    }
    status = GetVirtualDiskPhysicalPath(handle, &path_size, path);
    if (status != ERROR_SUCCESS) {
        FdiskErrorSetWindows(error, L"Could not read the attached image's physical disk path", status);
        free(path);
        return false;
    }
    const size_t path_chars = path_size / sizeof(wchar_t);
    const bool terminated = wcsnlen(path, path_chars) < path_chars;
    const bool parsed = terminated && ParsePhysicalDrivePath(path, disk_number);
    free(path);
    if (!parsed) {
        FdiskErrorSet(error, ERROR_INVALID_DATA,
            L"The virtual disk API returned an invalid physical disk path");
        return false;
    }
    return true;
}

bool FdiskImageCreate(const wchar_t* path, FdiskImageFormat format, uint64_t size_bytes,
    FdiskError* error) {
    VIRTUAL_STORAGE_TYPE storage_type = {0};
    if (!GetImageStorageType(path, &storage_type, error)) return false;
    if ((format == FDISK_IMAGE_FORMAT_VHD && storage_type.DeviceId != VIRTUAL_STORAGE_TYPE_DEVICE_VHD) ||
        (format == FDISK_IMAGE_FORMAT_VHDX && storage_type.DeviceId != VIRTUAL_STORAGE_TYPE_DEVICE_VHDX)) {
        FdiskErrorSet(error, ERROR_INVALID_NAME, L"Image format must match the .vhd or .vhdx file extension");
        return false;
    }
    if (size_bytes == 0 || size_bytes % 512u != 0) {
        FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Virtual disk capacity must be a positive multiple of 512 bytes");
        return false;
    }

    const DWORD attributes = GetFileAttributesW(path);
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        FdiskErrorSet(error, ERROR_FILE_EXISTS, L"Refusing to overwrite an existing image");
        return false;
    }
    const DWORD attribute_error = GetLastError();
    if (attribute_error != ERROR_FILE_NOT_FOUND && attribute_error != ERROR_PATH_NOT_FOUND) {
        FdiskErrorSetWindows(error, L"Could not inspect the image destination", attribute_error);
        return false;
    }

    CREATE_VIRTUAL_DISK_PARAMETERS parameters = {0};
    parameters.Version = CREATE_VIRTUAL_DISK_VERSION_1;
    parameters.Version1.MaximumSize = size_bytes;
    parameters.Version1.SectorSizeInBytes = 512;
    HANDLE handle = NULL;
    const DWORD status = CreateVirtualDisk(&storage_type, path, VIRTUAL_DISK_ACCESS_NONE,
        NULL, CREATE_VIRTUAL_DISK_FLAG_NONE, 0, &parameters, NULL, &handle);
    if (status != ERROR_SUCCESS) {
        FdiskErrorSetWindows(error, L"Could not create virtual disk image", status);
        if (!CloseHandleAfterError(handle, error)) return false;
        return false;
    }
    if (!CloseHandle(handle)) {
        FdiskErrorSetWindows(error, L"Image was created but its virtual disk handle could not be closed",
            GetLastError());
        return false;
    }
    return true;
}

bool FdiskImageResize(const wchar_t* path, uint64_t size_bytes, FdiskError* error) {
    if (size_bytes == 0 || size_bytes % 512u != 0) {
        FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Virtual disk capacity must be a positive multiple of 512 bytes");
        return false;
    }
    HANDLE handle = NULL;
    if (!OpenImage(path, VIRTUAL_DISK_ACCESS_METAOPS | VIRTUAL_DISK_ACCESS_GET_INFO |
            VIRTUAL_DISK_ACCESS_DETACH, &handle, error)) return false;

    GET_VIRTUAL_DISK_INFO info = {0};
    ULONG info_size = sizeof(info);
    info.Version = GET_VIRTUAL_DISK_INFO_IS_LOADED;
    DWORD status = GetVirtualDiskInformation(handle, &info_size, &info, NULL);
    if (status != ERROR_SUCCESS) {
        FdiskErrorSetWindows(error, L"Could not check whether the virtual disk is attached", status);
        (void)CloseHandleAfterError(handle, error);
        return false;
    }
    if (info.IsLoaded) {
        FdiskErrorSet(error, ERROR_BUSY, L"Detach the virtual disk image before resizing it");
        (void)CloseHandleAfterError(handle, error);
        return false;
    }

    RESIZE_VIRTUAL_DISK_PARAMETERS parameters = {0};
    parameters.Version = RESIZE_VIRTUAL_DISK_VERSION_1;
    parameters.Version1.NewSize = size_bytes;
    status = ResizeVirtualDisk(handle, RESIZE_VIRTUAL_DISK_FLAG_NONE, &parameters, NULL);
    if (status != ERROR_SUCCESS) {
        FdiskErrorSetWindows(error, L"Could not safely resize virtual disk image", status);
        (void)CloseHandleAfterError(handle, error);
        return false;
    }
    if (!CloseHandle(handle)) {
        FdiskErrorSetWindows(error, L"Image was resized but its virtual disk handle could not be closed",
            GetLastError());
        return false;
    }
    return true;
}

bool FdiskImageAttach(const wchar_t* path, FdiskImageSession* session, FdiskError* error) {
    if (session == NULL) {
        FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Missing virtual disk session");
        return false;
    }
    session->handle = NULL;
    session->disk_number = 0;
    HANDLE handle = NULL;
    if (!OpenImage(path, VIRTUAL_DISK_ACCESS_ATTACH_RW | VIRTUAL_DISK_ACCESS_DETACH |
            VIRTUAL_DISK_ACCESS_GET_INFO, &handle, error)) return false;

    ATTACH_VIRTUAL_DISK_PARAMETERS parameters = {0};
    parameters.Version = ATTACH_VIRTUAL_DISK_VERSION_1;
    const DWORD flags = ATTACH_VIRTUAL_DISK_FLAG_NO_DRIVE_LETTER |
        ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME;
    DWORD status = AttachVirtualDisk(handle, NULL, flags, 0, &parameters, NULL);
    const bool attached_here = status == ERROR_SUCCESS;
    if (status != ERROR_SUCCESS && status != ERROR_ALREADY_EXISTS) {
        FdiskErrorSetWindows(error, L"Could not attach virtual disk image", status);
        (void)CloseHandleAfterError(handle, error);
        return false;
    }
    if (!GetImageDiskNumber(handle, &session->disk_number, error)) {
        if (attached_here) {
            const DWORD detach_status = DetachVirtualDisk(handle, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
            if (detach_status != ERROR_SUCCESS) {
                AppendApiError(error, L"Could not detach the image after setup failed", detach_status);
            }
        }
        (void)CloseHandleAfterError(handle, error);
        return false;
    }
    session->handle = handle;
    return true;
}

bool FdiskImageDetach(const wchar_t* path, FdiskError* error) {
    HANDLE handle = NULL;
    if (!OpenImage(path, VIRTUAL_DISK_ACCESS_DETACH, &handle, error)) return false;
    const DWORD status = DetachVirtualDisk(handle, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
    if (status != ERROR_SUCCESS) {
        FdiskErrorSetWindows(error, L"Could not detach virtual disk image", status);
        (void)CloseHandleAfterError(handle, error);
        return false;
    }
    if (!CloseHandle(handle)) {
        FdiskErrorSetWindows(error, L"Image was detached but its virtual disk handle could not be closed",
            GetLastError());
        return false;
    }
    return true;
}

bool FdiskImageCloseSession(FdiskImageSession* session, FdiskError* error) {
    if (session == NULL) {
        FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Missing virtual disk session");
        return false;
    }
    if (session->handle == NULL || session->handle == INVALID_HANDLE_VALUE) return true;
    if (!CloseHandle(session->handle)) {
        FdiskErrorSetWindows(error, L"Could not close virtual disk session handle", GetLastError());
        return false;
    }
    session->handle = NULL;
    return true;
}
