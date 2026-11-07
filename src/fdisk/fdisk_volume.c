#include "fdisk_volume.h"

#include "fdisk_error.h"

#include <stdlib.h>
#include <wchar.h>

typedef struct FdiskVolumeLockList {
    FdiskLockedVolume* items;
    size_t count;
    size_t capacity;
} FdiskVolumeLockList;

static void AppendCleanupError(FdiskError* primary, const FdiskError* cleanup) {
    if (primary == NULL || cleanup == NULL || cleanup->message[0] == L'\0') return;
    wchar_t combined[FDISK_ERROR_CAPACITY];
    if (primary->message[0] == L'\0') {
        FdiskErrorSet(primary, cleanup->code, cleanup->message);
        return;
    }
    (void)swprintf_s(combined, _countof(combined), L"%ls; cleanup also failed: %ls",
        primary->message, cleanup->message);
    FdiskErrorSet(primary, primary->code, combined);
}

bool FdiskUnlockDiskVolumes(FdiskVolumeLocks* locks, FdiskError* error) {
    if (locks == NULL) {
        FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Invalid volume lock set");
        return false;
    }
    FdiskErrorClear(error);
    bool success = true;
    FdiskLockedVolume* volumes = locks->items;
    for (size_t i = locks->count; i > 0; --i) {
        FdiskLockedVolume* volume = &volumes[i - 1];
        if (volume->handle == INVALID_HANDLE_VALUE) continue;
        if (volume->locked) {
            DWORD bytes_returned = 0;
            if (!DeviceIoControl(volume->handle, FSCTL_UNLOCK_VOLUME,
                    NULL, 0, NULL, 0, &bytes_returned, NULL) && success) {
                FdiskErrorSetWindows(error, L"Could not unlock a dismounted volume", GetLastError());
                success = false;
            }
        }
        if (!CloseHandle(volume->handle) && success) {
            FdiskErrorSetWindows(error, L"Could not close a volume handle", GetLastError());
            success = false;
        }
    }
    free(locks->items);
    locks->items = NULL;
    locks->count = 0;
    return success;
}

static bool AppendVolumeHandle(
    FdiskVolumeLockList* list, HANDLE handle, FdiskError* error) {
    if (list->count == list->capacity) {
        if (list->capacity > SIZE_MAX / 2 / sizeof(*list->items)) {
            FdiskErrorSet(error, ERROR_NOT_ENOUGH_MEMORY, L"Too many volumes use the target disk");
            return false;
        }
        const size_t capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        FdiskLockedVolume* resized = (FdiskLockedVolume*)realloc(
            list->items, capacity * sizeof(*list->items));
        if (resized == NULL) {
            FdiskErrorSet(error, ERROR_NOT_ENOUGH_MEMORY, L"Out of memory locking disk volumes");
            return false;
        }
        list->items = resized;
        list->capacity = capacity;
    }
    list->items[list->count++] = (FdiskLockedVolume){handle, false};
    return true;
}

static bool QueryVolumeExtents(HANDLE volume, uint32_t disk_number, bool* belongs, FdiskError* error) {
    const DWORD header_size = (DWORD)FIELD_OFFSET(VOLUME_DISK_EXTENTS, Extents);
    DWORD capacity = header_size + 4 * (DWORD)sizeof(DISK_EXTENT);

    *belongs = false;
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        BYTE* buffer = (BYTE*)malloc(capacity);
        if (buffer == NULL) {
            FdiskErrorSet(error, ERROR_NOT_ENOUGH_MEMORY, L"Out of memory checking volume extents");
            return false;
        }
        DWORD returned = 0;
        const BOOL queried = DeviceIoControl(volume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
            NULL, 0, buffer, capacity, &returned, NULL);
        DWORD code = queried ? ERROR_SUCCESS : GetLastError();
        if (!queried) {
            free(buffer);
            if (code != ERROR_MORE_DATA && code != ERROR_INSUFFICIENT_BUFFER) {
                FdiskErrorSetWindows(error, L"Could not inspect volume disk extents before writing", code);
                return false;
            }
            if (capacity >= 4u * 1024u * 1024u) break;
            capacity = returned > capacity && returned <= 4u * 1024u * 1024u
                ? returned
                : capacity * 2;
            continue;
        }

        if (returned < header_size) {
            free(buffer);
            FdiskErrorSet(error, ERROR_INVALID_DATA, L"Volume returned an incomplete disk extent list");
            return false;
        }
        const VOLUME_DISK_EXTENTS* extents = (const VOLUME_DISK_EXTENTS*)buffer;
        const size_t available = (returned - header_size) / sizeof(DISK_EXTENT);
        if ((size_t)extents->NumberOfDiskExtents > available) {
            free(buffer);
            FdiskErrorSet(error, ERROR_INVALID_DATA, L"Volume extent count exceeds returned data");
            return false;
        }
        for (DWORD i = 0; i < extents->NumberOfDiskExtents; ++i) {
            if (extents->Extents[i].DiskNumber == disk_number) {
                *belongs = true;
                break;
            }
        }
        free(buffer);
        return true;
    }

    FdiskErrorSet(error, ERROR_INSUFFICIENT_BUFFER, L"Volume extent list exceeds the supported size");
    return false;
}

static bool OpenVolumePath(const wchar_t* volume_path, wchar_t* open_path, size_t count) {
    if (wcsncpy_s(open_path, count, volume_path, _TRUNCATE) != 0) return false;
    const size_t length = wcslen(open_path);
    if (length != 0 && open_path[length - 1] == L'\\') open_path[length - 1] = L'\0';
    return length > 1;
}

bool FdiskLockDiskVolumes(uint32_t disk_number, FdiskVolumeLocks* locks, FdiskError* error) {
    if (locks == NULL) {
        FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Invalid volume lock output");
        return false;
    }
    locks->items = NULL;
    locks->count = 0;
    FdiskErrorClear(error);

    FdiskVolumeLockList list = {0};
    wchar_t volume_path[MAX_PATH];
    HANDLE search = FindFirstVolumeW(volume_path, _countof(volume_path));
    if (search == INVALID_HANDLE_VALUE) {
        FdiskErrorSetWindows(error, L"Could not enumerate volumes before writing", GetLastError());
        return false;
    }

    bool success = true;
    for (;;) {
        wchar_t open_path[MAX_PATH];
        if (!OpenVolumePath(volume_path, open_path, _countof(open_path))) {
            FdiskErrorSet(error, ERROR_INVALID_NAME, L"Windows returned an invalid volume path");
            success = false;
            break;
        }

        HANDLE query_handle = CreateFileW(open_path, GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (query_handle == INVALID_HANDLE_VALUE) {
            FdiskErrorSetWindows(error, L"Could not open a volume to inspect its disk extents", GetLastError());
            success = false;
            break;
        }

        bool belongs = false;
        const bool queried = QueryVolumeExtents(query_handle, disk_number, &belongs, error);
        if (!CloseHandle(query_handle) && queried) {
            FdiskErrorSetWindows(error, L"Could not close a volume inspection handle", GetLastError());
            success = false;
            break;
        }
        if (!queried) {
            success = false;
            break;
        }

        if (belongs) {
            HANDLE write_handle = CreateFileW(open_path, GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
            if (write_handle == INVALID_HANDLE_VALUE) {
                FdiskErrorSetWindows(error, L"Could not open a target volume for locking", GetLastError());
                success = false;
                break;
            }
            if (!AppendVolumeHandle(&list, write_handle, error)) {
                (void)CloseHandle(write_handle);
                success = false;
                break;
            }
            DWORD bytes_returned = 0;
            if (!DeviceIoControl(write_handle, FSCTL_LOCK_VOLUME,
                    NULL, 0, NULL, 0, &bytes_returned, NULL)) {
                FdiskErrorSetWindows(error, L"Could not lock a target volume", GetLastError());
                success = false;
                break;
            }
            list.items[list.count - 1].locked = true;
        }

        if (!FindNextVolumeW(search, volume_path, _countof(volume_path))) {
            const DWORD code = GetLastError();
            if (code != ERROR_NO_MORE_FILES) {
                FdiskErrorSetWindows(error, L"Could not finish volume enumeration", code);
                success = false;
            }
            break;
        }
    }

    if (!FindVolumeClose(search) && success) {
        FdiskErrorSetWindows(error, L"Could not close volume enumeration", GetLastError());
        success = false;
    }

    if (success) {
        for (size_t i = 0; i < list.count; ++i) {
            DWORD bytes_returned = 0;
            if (!DeviceIoControl(list.items[i].handle, FSCTL_DISMOUNT_VOLUME,
                    NULL, 0, NULL, 0, &bytes_returned, NULL)) {
                FdiskErrorSetWindows(error, L"Could not dismount a locked target volume", GetLastError());
                success = false;
                break;
            }
        }
    }

    if (!success) {
        FdiskError cleanup_error = {0};
        FdiskVolumeLocks cleanup = {list.items, list.count};
        (void)FdiskUnlockDiskVolumes(&cleanup, &cleanup_error);
        AppendCleanupError(error, &cleanup_error);
        return false;
    }

    locks->items = list.items;
    locks->count = list.count;
    return true;
}
