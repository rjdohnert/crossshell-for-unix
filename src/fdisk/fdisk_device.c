#include "fdisk_device.h"

#include "fdisk_error.h"
#include "fdisk_volume.h"

#include <ntddstor.h>
#include <setupapi.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static const GUID DISK_INTERFACE_GUID = {
    0x53f56307, 0xb6bf, 0x11d0, {0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b}
};

static bool AppendDiskNumber(uint32_t** numbers, size_t* count, size_t* capacity, uint32_t number) {
    for (size_t i = 0; i < *count; ++i) {
        if ((*numbers)[i] == number) return true;
    }

    if (*count == *capacity) {
        if (*capacity > SIZE_MAX / 2 / sizeof(**numbers)) return false;
        const size_t new_capacity = *capacity == 0 ? 8 : *capacity * 2;
        uint32_t* resized = (uint32_t*)realloc(*numbers, new_capacity * sizeof(**numbers));
        if (resized == NULL) return false;
        *numbers = resized;
        *capacity = new_capacity;
    }

    (*numbers)[(*count)++] = number;
    return true;
}

static int CompareDiskNumbers(const void* left, const void* right) {
    const uint32_t a = *(const uint32_t*)left;
    const uint32_t b = *(const uint32_t*)right;
    return (a > b) - (a < b);
}

static bool AllocationSizeIsValid(size_t count, size_t item_size) {
    return count == 0 || item_size <= SIZE_MAX / count;
}

bool FdiskEnumerateDiskNumbers(uint32_t** numbers, size_t* count, FdiskError* error) {
    if (numbers == NULL || count == NULL) {
        FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Invalid disk enumeration output");
        return false;
    }

    *numbers = NULL;
    *count = 0;
    FdiskErrorClear(error);

    HDEVINFO devices = SetupDiGetClassDevsW(
        &DISK_INTERFACE_GUID, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devices == INVALID_HANDLE_VALUE) {
        FdiskErrorSetWindows(error, L"Could not enumerate disk devices", GetLastError());
        return false;
    }

    uint32_t* found = NULL;
    size_t found_count = 0;
    size_t capacity = 0;
    bool success = false;

    for (DWORD index = 0;; ++index) {
        SP_DEVICE_INTERFACE_DATA interface_data = {0};
        interface_data.cbSize = sizeof(interface_data);
        if (!SetupDiEnumDeviceInterfaces(devices, NULL, &DISK_INTERFACE_GUID, index, &interface_data)) {
            const DWORD code = GetLastError();
            if (code == ERROR_NO_MORE_ITEMS) {
                success = true;
                break;
            }
            FdiskErrorSetWindows(error, L"Could not enumerate disk interfaces", code);
            break;
        }

        DWORD required = 0;
        (void)SetupDiGetDeviceInterfaceDetailW(devices, &interface_data, NULL, 0, &required, NULL);
        DWORD code = GetLastError();
        if (required < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || code != ERROR_INSUFFICIENT_BUFFER) {
            FdiskErrorSetWindows(error, L"Could not size disk interface details", code);
            break;
        }

        SP_DEVICE_INTERFACE_DETAIL_DATA_W* detail =
            (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)malloc(required);
        if (detail == NULL) {
            FdiskErrorSet(error, ERROR_NOT_ENOUGH_MEMORY, L"Out of memory enumerating disk interfaces");
            break;
        }
        detail->cbSize = sizeof(*detail);

        if (!SetupDiGetDeviceInterfaceDetailW(
                devices, &interface_data, detail, required, NULL, NULL)) {
            code = GetLastError();
            free(detail);
            FdiskErrorSetWindows(error, L"Could not read disk interface details", code);
            break;
        }

        HANDLE device = CreateFileW(
            detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, 0, NULL);
        free(detail);
        if (device == INVALID_HANDLE_VALUE) {
            FdiskErrorSetWindows(error, L"Could not query a disk device", GetLastError());
            break;
        }

        STORAGE_DEVICE_NUMBER device_number = {0};
        DWORD bytes_returned = 0;
        const BOOL queried = DeviceIoControl(
            device, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0,
            &device_number, sizeof(device_number), &bytes_returned, NULL);
        code = queried ? ERROR_SUCCESS : GetLastError();
        const BOOL closed = CloseHandle(device);
        if (!queried) {
            FdiskErrorSetWindows(error, L"Could not identify a disk device", code);
            break;
        }
        if (!closed) {
            FdiskErrorSetWindows(error, L"Could not close a disk device handle", GetLastError());
            break;
        }
        if (bytes_returned < sizeof(device_number)) {
            FdiskErrorSet(error, ERROR_INVALID_DATA, L"Disk device returned an incomplete device number");
            break;
        }
        if (device_number.DeviceType != FILE_DEVICE_DISK) continue;

        if (!AppendDiskNumber(&found, &found_count, &capacity, device_number.DeviceNumber)) {
            FdiskErrorSet(error, ERROR_NOT_ENOUGH_MEMORY, L"Out of memory collecting disk numbers");
            break;
        }
    }

    (void)SetupDiDestroyDeviceInfoList(devices);
    if (!success) {
        free(found);
        return false;
    }

    qsort(found, found_count, sizeof(*found), CompareDiskNumbers);
    *numbers = found;
    *count = found_count;
    return true;
}

static bool ReadLayout(HANDLE device, BYTE** buffer, DWORD* bytes_returned, FdiskError* error) {
    const DWORD initial_size = (DWORD)(FIELD_OFFSET(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry) +
        16 * sizeof(PARTITION_INFORMATION_EX));
    const DWORD maximum_size = 64u * 1024u * 1024u;
    DWORD capacity = initial_size;

    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        BYTE* data = (BYTE*)malloc(capacity);
        if (data == NULL) {
            FdiskErrorSet(error, ERROR_NOT_ENOUGH_MEMORY, L"Out of memory reading partition layout");
            return false;
        }

        DWORD returned = 0;
        if (DeviceIoControl(device, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0,
                data, capacity, &returned, NULL)) {
            *buffer = data;
            *bytes_returned = returned;
            return true;
        }

        const DWORD code = GetLastError();
        free(data);
        if (code != ERROR_INSUFFICIENT_BUFFER && code != ERROR_MORE_DATA) {
            FdiskErrorSetWindows(error, L"Could not read partition layout", code);
            return false;
        }
        if (capacity >= maximum_size) break;
        capacity = capacity > maximum_size / 2 ? maximum_size : capacity * 2;
    }

    FdiskErrorSet(error, ERROR_INSUFFICIENT_BUFFER, L"Partition layout exceeds the supported size");
    return false;
}

void FdiskFreeDisk(FdiskDisk* disk) {
    if (disk == NULL) return;
    free(disk->partitions);
    memset(disk, 0, sizeof(*disk));
}

bool FdiskReadDisk(uint32_t number, FdiskDisk* disk, FdiskError* error) {
    if (disk == NULL) {
        FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Invalid disk output");
        return false;
    }

    FdiskErrorClear(error);
    wchar_t path[64];
    if (swprintf_s(path, _countof(path), L"\\\\.\\PhysicalDrive%lu", (unsigned long)number) < 0) {
        FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Invalid disk number");
        return false;
    }

    HANDLE device = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (device == INVALID_HANDLE_VALUE) {
        FdiskErrorSetWindows(error, L"Could not open disk", GetLastError());
        return false;
    }

    FdiskDisk result = {0};
    BYTE* layout_buffer = NULL;
    DWORD layout_bytes = 0;
    bool success = false;

    DISK_GEOMETRY_EX geometry = {0};
    DWORD geometry_bytes = 0;
    if (!DeviceIoControl(device, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0,
            &geometry, sizeof(geometry), &geometry_bytes, NULL)) {
        FdiskErrorSetWindows(error, L"Could not read disk geometry", GetLastError());
        goto cleanup;
    }
    if (geometry_bytes < FIELD_OFFSET(DISK_GEOMETRY_EX, Data) ||
        geometry.DiskSize.QuadPart <= 0 || geometry.Geometry.BytesPerSector == 0) {
        FdiskErrorSet(error, ERROR_INVALID_DATA, L"Disk returned invalid geometry");
        goto cleanup;
    }

    if (!ReadLayout(device, &layout_buffer, &layout_bytes, error)) goto cleanup;

    const size_t layout_header = FIELD_OFFSET(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry);
    if (layout_bytes < layout_header) {
        FdiskErrorSet(error, ERROR_INVALID_DATA, L"Disk returned an incomplete partition layout");
        goto cleanup;
    }

    const DRIVE_LAYOUT_INFORMATION_EX* layout = (const DRIVE_LAYOUT_INFORMATION_EX*)layout_buffer;
    const size_t available_entries = (layout_bytes - layout_header) / sizeof(PARTITION_INFORMATION_EX);
    if ((size_t)layout->PartitionCount > available_entries) {
        FdiskErrorSet(error, ERROR_INVALID_DATA, L"Partition count exceeds the returned layout data");
        goto cleanup;
    }
    if (layout->PartitionStyle != PARTITION_STYLE_MBR &&
        layout->PartitionStyle != PARTITION_STYLE_GPT &&
        layout->PartitionStyle != PARTITION_STYLE_RAW) {
        FdiskErrorSet(error, ERROR_INVALID_DATA, L"Disk returned an unknown partition style");
        goto cleanup;
    }
    if (!AllocationSizeIsValid((size_t)layout->PartitionCount, sizeof(*result.partitions))) {
        FdiskErrorSet(error, ERROR_NOT_ENOUGH_MEMORY, L"Partition count is too large");
        goto cleanup;
    }

    result.partitions = layout->PartitionCount == 0
        ? NULL
        : (FdiskPartition*)calloc(layout->PartitionCount, sizeof(*result.partitions));
    if (layout->PartitionCount != 0 && result.partitions == NULL) {
        FdiskErrorSet(error, ERROR_NOT_ENOUGH_MEMORY, L"Out of memory storing partition data");
        goto cleanup;
    }

    result.number = number;
    result.size_bytes = (uint64_t)geometry.DiskSize.QuadPart;
    result.sector_size = geometry.Geometry.BytesPerSector;
    result.sectors_per_track = geometry.Geometry.SectorsPerTrack;
    result.tracks_per_cylinder = geometry.Geometry.TracksPerCylinder;
    result.style = layout->PartitionStyle;
    if (result.style == PARTITION_STYLE_MBR) {
        result.mbr_signature = layout->Mbr.Signature;
    } else if (result.style == PARTITION_STYLE_GPT) {
        result.gpt_id = layout->Gpt.DiskId;
        result.gpt_max_partition_count = layout->Gpt.MaxPartitionCount;
    }
    if (swprintf_s(result.device_path, _countof(result.device_path),
            L"\\\\.\\PhysicalDrive%lu", (unsigned long)number) < 0) {
        FdiskErrorSet(error, ERROR_INVALID_DATA, L"Could not format disk path");
        goto cleanup;
    }

    const PARTITION_INFORMATION_EX* entries = layout->PartitionEntry;
    const uint64_t disk_bytes = result.size_bytes;
    for (DWORD i = 0; i < layout->PartitionCount; ++i) {
        const PARTITION_INFORMATION_EX* entry = &entries[i];
        const LONGLONG start = entry->StartingOffset.QuadPart;
        const LONGLONG length = entry->PartitionLength.QuadPart;

        if (entry->PartitionNumber == 0 && start == 0 && length == 0) continue;
        if (entry->PartitionNumber == 0 || start < 0 || length <= 0 ||
            (uint64_t)start > disk_bytes || (uint64_t)length > disk_bytes - (uint64_t)start ||
            (uint64_t)start % result.sector_size != 0 ||
            (uint64_t)length % result.sector_size != 0) {
            FdiskErrorSet(error, ERROR_INVALID_DATA, L"Disk returned an invalid partition extent");
            goto cleanup;
        }

        FdiskPartition* partition = &result.partitions[result.partition_count++];
        partition->native = *entry;
        partition->number = entry->PartitionNumber;
        partition->start_sector = (uint64_t)start / result.sector_size;
        partition->sector_count = (uint64_t)length / result.sector_size;
        partition->size_bytes = (uint64_t)length;
        if (result.style == PARTITION_STYLE_MBR) {
            partition->bootable = entry->Mbr.BootIndicator != FALSE;
            partition->mbr_type = entry->Mbr.PartitionType;
        } else if (result.style == PARTITION_STYLE_GPT) {
            partition->gpt_type = entry->Gpt.PartitionType;
            partition->gpt_id = entry->Gpt.PartitionId;
            for (size_t name_index = 0; name_index < _countof(entry->Gpt.Name); ++name_index) {
                partition->name[name_index] = entry->Gpt.Name[name_index];
                if (entry->Gpt.Name[name_index] == L'\0') break;
            }
        }
    }

    if (!CloseHandle(device)) {
        FdiskErrorSetWindows(error, L"Could not close disk handle", GetLastError());
        device = INVALID_HANDLE_VALUE;
        goto cleanup;
    }
    device = INVALID_HANDLE_VALUE;
    success = true;

cleanup:
    free(layout_buffer);
    if (device != INVALID_HANDLE_VALUE) (void)CloseHandle(device);
    if (success) {
        *disk = result;
        result.partitions = NULL;
    }
    free(result.partitions);
    return success;
}

static bool WaitForGptInitialization(
    uint32_t number, PARTITION_INFORMATION_EX* msr_entry, FdiskError* error) {
    static const GUID GPT_MICROSOFT_RESERVED = {
        0xe3c9e316, 0x0b5c, 0x4db8, {0x81, 0x7d, 0xf9, 0x2d, 0xf0, 0x02, 0x15, 0xae}
    };
    const DWORD timeout_ms = 30000;
    const DWORD interval_ms = 250;
    DWORD elapsed = 0;

    while (elapsed < timeout_ms) {
        FdiskDisk current = {0};
        FdiskError read_error = {0};
        if (FdiskReadDisk(number, &current, &read_error)) {
            bool msr_found = false;
            if (current.style == PARTITION_STYLE_GPT) {
                for (size_t i = 0; i < current.partition_count; ++i) {
                    if (IsEqualGUID(&current.partitions[i].gpt_type, &GPT_MICROSOFT_RESERVED)) {
                        msr_found = true;
                        *msr_entry = current.partitions[i].native;
                        break;
                    }
                }
            }
            FdiskFreeDisk(&current);
            if (msr_found) return true;
        }
        Sleep(interval_ms);
        elapsed += interval_ms;
    }

    FdiskErrorSet(error, ERROR_TIMEOUT,
        L"Disk initialization succeeded, but the GPT system partition did not arrive before timeout; the new layout was not written");
    return false;
}

static bool RelocateNewGptPartitions(
    FdiskDisk* disk,
    DRIVE_LAYOUT_INFORMATION_EX* layout,
    uint64_t disk_bytes,
    uint32_t sector_size,
    const PARTITION_INFORMATION_EX* msr_entry,
    bool* relocated,
    FdiskError* error) {
    *relocated = false;
    if (layout->PartitionCount == 0) return true;
    if (sector_size == 0 || msr_entry->StartingOffset.QuadPart < 0 ||
        msr_entry->PartitionLength.QuadPart <= 0) {
        FdiskErrorSet(error, ERROR_INVALID_DATA, L"GPT initialization returned an invalid MSR extent");
        return false;
    }

    const uint64_t msr_start = (uint64_t)msr_entry->StartingOffset.QuadPart;
    const uint64_t msr_length = (uint64_t)msr_entry->PartitionLength.QuadPart;
    if (msr_length > UINT64_MAX - msr_start) {
        FdiskErrorSet(error, ERROR_INVALID_DATA, L"GPT initialization returned an overflowing MSR extent");
        return false;
    }

    const uint64_t alignment = (1024u * 1024u + sector_size - 1) / sector_size;
    const uint64_t msr_end_bytes = msr_start + msr_length;
    const uint64_t msr_end_sectors = msr_end_bytes / sector_size +
        (msr_end_bytes % sector_size != 0 ? 1 : 0);
    const uint64_t remainder = msr_end_sectors % alignment;
    const uint64_t first_usable_sector = remainder == 0
        ? msr_end_sectors
        : msr_end_sectors + alignment - remainder;
    if (first_usable_sector > UINT64_MAX / sector_size) {
        FdiskErrorSet(error, ERROR_INVALID_DATA, L"GPT usable extent exceeds supported size");
        return false;
    }
    const uint64_t first_usable_byte = first_usable_sector * sector_size;
    uint64_t first_partition_byte = UINT64_MAX;
    for (DWORD i = 0; i < layout->PartitionCount; ++i) {
        const LONGLONG start = layout->PartitionEntry[i].StartingOffset.QuadPart;
        if (start < 0) {
            FdiskErrorSet(error, ERROR_INVALID_DATA, L"Staged GPT partition has a negative offset");
            return false;
        }
        if ((uint64_t)start < first_partition_byte) first_partition_byte = (uint64_t)start;
    }
    if (first_partition_byte >= first_usable_byte) return true;

    const uint64_t shift = first_usable_byte - first_partition_byte;
    const uint64_t tail_bytes = (uint64_t)34 * sector_size;
    const uint64_t usable_end = disk_bytes > tail_bytes ? disk_bytes - tail_bytes : 0;
    for (DWORD i = 0; i < layout->PartitionCount; ++i) {
        PARTITION_INFORMATION_EX* partition = &layout->PartitionEntry[i];
        const LONGLONG start = partition->StartingOffset.QuadPart;
        const LONGLONG length = partition->PartitionLength.QuadPart;
        if (start < 0 || length <= 0 ||
            (uint64_t)start > UINT64_MAX - shift ||
            (uint64_t)length > usable_end ||
            (uint64_t)start + shift > usable_end - (uint64_t)length) {
            FdiskErrorSet(error, ERROR_DISK_FULL,
                L"GPT system partition leaves insufficient space for the staged layout");
            return false;
        }
        const uint64_t shifted_start = (uint64_t)start + shift;
        if (shifted_start > LLONG_MAX) {
            FdiskErrorSet(error, ERROR_INVALID_DATA, L"GPT partition offset exceeds supported size");
            return false;
        }
        partition->StartingOffset.QuadPart = (LONGLONG)shifted_start;
        disk->partitions[i].start_sector = shifted_start / sector_size;
        disk->partitions[i].native.StartingOffset.QuadPart = (LONGLONG)shifted_start;
    }
    *relocated = true;
    return true;
}

bool FdiskWriteDiskLayout(FdiskDisk* disk, FdiskError* error) {
    FdiskErrorClear(error);
    if (disk == NULL || (disk->partition_count != 0 && disk->partitions == NULL) ||
        (disk->style != PARTITION_STYLE_MBR && disk->style != PARTITION_STYLE_GPT)) {
        FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Invalid disk layout");
        return false;
    }
    const bool add_gpt_msr = disk->initialize_layout && disk->style == PARTITION_STYLE_GPT;
    if (add_gpt_msr && disk->partition_count == SIZE_MAX) {
        FdiskErrorSet(error, ERROR_NOT_ENOUGH_MEMORY, L"Partition layout is too large");
        return false;
    }
    const size_t entry_capacity = disk->partition_count + (add_gpt_msr ? 1 : 0);
    if (entry_capacity > MAXDWORD ||
        !AllocationSizeIsValid(entry_capacity, sizeof(PARTITION_INFORMATION_EX)) ||
        entry_capacity >
            (MAXDWORD - FIELD_OFFSET(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry)) /
            sizeof(PARTITION_INFORMATION_EX)) {
        FdiskErrorSet(error, ERROR_NOT_ENOUGH_MEMORY, L"Partition layout is too large");
        return false;
    }

    const size_t layout_size = FIELD_OFFSET(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry) +
        entry_capacity * sizeof(PARTITION_INFORMATION_EX);
    DRIVE_LAYOUT_INFORMATION_EX* layout = (DRIVE_LAYOUT_INFORMATION_EX*)calloc(1, layout_size);
    if (layout == NULL) {
        FdiskErrorSet(error, ERROR_NOT_ENOUGH_MEMORY, L"Out of memory preparing partition layout");
        return false;
    }

    layout->PartitionStyle = disk->style;
    layout->PartitionCount = (DWORD)disk->partition_count;
    if (disk->style == PARTITION_STYLE_MBR) {
        layout->Mbr.Signature = disk->mbr_signature;
    } else {
        layout->Gpt.DiskId = disk->gpt_id;
        layout->Gpt.MaxPartitionCount = disk->gpt_max_partition_count != 0
            ? disk->gpt_max_partition_count
            : 128;
    }
    for (size_t i = 0; i < disk->partition_count; ++i) {
        layout->PartitionEntry[i] = disk->partitions[i].native;
        layout->PartitionEntry[i].PartitionStyle = disk->style;
        layout->PartitionEntry[i].PartitionNumber = disk->partitions[i].number;
        layout->PartitionEntry[i].RewritePartition = TRUE;
    }

    wchar_t path[64];
    if (swprintf_s(path, _countof(path), L"\\\\.\\PhysicalDrive%lu",
            (unsigned long)disk->number) < 0) {
        free(layout);
        FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Invalid disk number");
        return false;
    }

    FdiskVolumeLocks volume_locks = {0};
    bool success = false;
    bool disk_initialized = false;
    bool relocated = false;
    DWORD bytes_returned = 0;
    HANDLE device = INVALID_HANDLE_VALUE;
    if (!FdiskLockDiskVolumes(disk->number, &volume_locks, error)) {
        free(layout);
        return false;
    }

    device = CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (device == INVALID_HANDLE_VALUE) {
        const DWORD code = GetLastError();
        FdiskErrorSetWindows(error, L"Could not open disk for writing", code);
        goto cleanup;
    }

    if (disk->initialize_layout) {
        CREATE_DISK create = {0};
        create.PartitionStyle = disk->style;
        if (disk->style == PARTITION_STYLE_MBR) {
            create.Mbr.Signature = disk->mbr_signature;
        } else {
            create.Gpt.DiskId = disk->gpt_id;
            create.Gpt.MaxPartitionCount = layout->Gpt.MaxPartitionCount;
        }
        if (!DeviceIoControl(device, IOCTL_DISK_CREATE_DISK, &create, sizeof(create),
                NULL, 0, &bytes_returned, NULL)) {
            const DWORD code = GetLastError();
            FdiskErrorSetWindows(error, L"Could not initialize partition table", code);
            goto cleanup;
        }
        disk_initialized = true;
        if (disk->style == PARTITION_STYLE_GPT) {
            if (!CloseHandle(device)) {
                device = INVALID_HANDLE_VALUE;
                FdiskErrorSetWindows(error, L"Could not close initialized disk", GetLastError());
                goto cleanup;
            }
            device = INVALID_HANDLE_VALUE;
            PARTITION_INFORMATION_EX msr_entry = {0};
            if (!WaitForGptInitialization(disk->number, &msr_entry, error)) goto cleanup;
            if (!RelocateNewGptPartitions(disk, layout, disk->size_bytes,
                    disk->sector_size, &msr_entry, &relocated, error)) goto cleanup;
            device = CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (device == INVALID_HANDLE_VALUE) {
                FdiskErrorSetWindows(error, L"Could not reopen initialized disk", GetLastError());
                goto cleanup;
            }
            layout->PartitionEntry[layout->PartitionCount++] = msr_entry;
        }
    }

    for (DWORD i = 1; i < layout->PartitionCount; ++i) {
        PARTITION_INFORMATION_EX current = layout->PartitionEntry[i];
        DWORD position = i;
        while (position > 0 &&
            layout->PartitionEntry[position - 1].StartingOffset.QuadPart >
                current.StartingOffset.QuadPart) {
            layout->PartitionEntry[position] = layout->PartitionEntry[position - 1];
            --position;
        }
        layout->PartitionEntry[position] = current;
    }
    DWORD primary_number = 1;
    DWORD logical_number = 5;
    for (DWORD i = 0; i < layout->PartitionCount; ++i) {
        const bool logical = disk->style == PARTITION_STYLE_MBR &&
            layout->PartitionEntry[i].PartitionNumber >= 5;
        layout->PartitionEntry[i].PartitionNumber = logical
            ? logical_number++ : primary_number++;
        layout->PartitionEntry[i].RewritePartition = TRUE;
    }

    if (!DeviceIoControl(device, IOCTL_DISK_SET_DRIVE_LAYOUT_EX,
            layout, (DWORD)layout_size, NULL, 0, &bytes_returned, NULL)) {
        const DWORD code = GetLastError();
        if (disk_initialized) {
            FdiskErrorSetWindows(error,
                L"Disk initialization succeeded, but writing the requested partition layout failed",
                code);
        } else {
            FdiskErrorSetWindows(error, L"Could not write partition layout", code);
        }
        goto cleanup;
    }
    if (relocated) {
        wprintf(L"The GPT system partition required reserved space; staged partitions were moved to avoid overlap.\n");
    }

    if (!DeviceIoControl(device, IOCTL_DISK_UPDATE_PROPERTIES,
            NULL, 0, NULL, 0, &bytes_returned, NULL)) {
        FdiskErrorSetWindows(error, L"Partition table was written, but disk properties could not be refreshed",
            GetLastError());
        goto cleanup;
    }
    success = true;

cleanup:
    if (device != INVALID_HANDLE_VALUE && !CloseHandle(device) && success) {
        FdiskErrorSetWindows(error, L"Partition table was written, but disk handle could not be closed",
            GetLastError());
        success = false;
    }
    FdiskError unlock_error = {0};
    if (!FdiskUnlockDiskVolumes(&volume_locks, &unlock_error)) {
        if (success) {
            FdiskErrorSet(&unlock_error, unlock_error.code,
                L"Partition table was written, but volume cleanup failed");
            if (error != NULL) *error = unlock_error;
            success = false;
        } else if (error != NULL && unlock_error.message[0] != L'\0') {
            wchar_t combined[FDISK_ERROR_CAPACITY];
            (void)swprintf_s(combined, _countof(combined), L"%ls; cleanup also failed: %ls",
                error->message, unlock_error.message);
            FdiskErrorSet(error, error->code, combined);
        }
    }
    free(layout);
    return success;
}
