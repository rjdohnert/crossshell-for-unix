#include "fdisk_editor.h"

#include "fdisk_device.h"
#include "fdisk_error.h"
#include "fdisk_format.h"

#include <errno.h>
#include <objbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static const GUID GPT_BASIC_DATA = {
    0xebd0a0a2, 0xb9e5, 0x4433, {0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7}
};
static const GUID GPT_EFI_SYSTEM = {
    0xc12a7328, 0xf81f, 0x11d2, {0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b}
};
static const GUID GPT_MICROSOFT_RESERVED = {
    0xe3c9e316, 0x0b5c, 0x4db8, {0x81, 0x7d, 0xf9, 0x2d, 0xf0, 0x02, 0x15, 0xae}
};
static const GUID GPT_WINDOWS_RECOVERY = {
    0xde94bba4, 0x06d1, 0x4d40, {0xa1, 0x6a, 0xbf, 0xd5, 0x01, 0x79, 0xd6, 0xac}
};

static bool ReadLine(const wchar_t* prompt, wchar_t* line, size_t count) {
    wprintf(L"%ls", prompt);
    if (fgetws(line, (int)count, stdin) == NULL) return false;
    size_t length = wcslen(line);
    if (length != 0 && line[length - 1] == L'\n') line[--length] = L'\0';
    else if (!feof(stdin)) {
        wint_t character;
        while ((character = getwchar()) != WEOF && character != L'\n') {}
        fwprintf(stderr, L"fdisk: input is too long.\n");
        return false;
    }
    if (length != 0 && line[length - 1] == L'\r') line[length - 1] = L'\0';
    return true;
}

static bool ParseUnsigned(const wchar_t* text, uint64_t* value) {
    if (text == NULL || *text == L'\0' || value == NULL) return false;
    uint64_t result = 0;
    for (const wchar_t* cursor = text; *cursor != L'\0'; ++cursor) {
        if (*cursor < L'0' || *cursor > L'9') return false;
        const uint64_t digit = (uint64_t)(*cursor - L'0');
        if (result > (UINT64_MAX - digit) / 10) return false;
        result = result * 10 + digit;
    }
    *value = result;
    return true;
}

static bool ReadNumber(const wchar_t* prompt, uint64_t minimum, uint64_t maximum, uint64_t* value) {
    wchar_t line[64];
    if (!ReadLine(prompt, line, _countof(line))) return false;
    if (!ParseUnsigned(line, value) || *value < minimum || *value > maximum) {
        fwprintf(stderr, L"fdisk: enter a decimal value from %llu to %llu.\n",
            (unsigned long long)minimum, (unsigned long long)maximum);
        return false;
    }
    return true;
}

static uint64_t AlignUp(uint64_t value, uint64_t alignment) {
    const uint64_t remainder = value % alignment;
    if (remainder == 0) return value;
    if (value > UINT64_MAX - (alignment - remainder)) return UINT64_MAX;
    return value + alignment - remainder;
}

static bool IsExtendedPartition(const FdiskPartition* partition) {
    return partition->native.PartitionStyle == PARTITION_STYLE_MBR &&
        (partition->mbr_type == 0x05 || partition->mbr_type == 0x0f);
}

static bool IsLogicalPartition(const FdiskPartition* partition) {
    return partition->native.PartitionStyle == PARTITION_STYLE_MBR &&
        partition->native.PartitionNumber >= 5;
}

static size_t FindExtendedPartition(const FdiskDisk* disk) {
    for (size_t i = 0; i < disk->partition_count; ++i) {
        if (IsExtendedPartition(&disk->partitions[i])) return i;
    }
    return SIZE_MAX;
}

static size_t CountPrimaryMbrPartitions(const FdiskDisk* disk) {
    size_t count = 0;
    for (size_t i = 0; i < disk->partition_count; ++i) {
        if (!IsLogicalPartition(&disk->partitions[i])) ++count;
    }
    return count;
}

static bool FindFreeExtent(
    const FdiskDisk* disk,
    uint64_t needed_sectors,
    bool logical,
    uint64_t* start_sector) {
    const uint64_t sector_size = disk->sector_size;
    if (sector_size == 0 || needed_sectors == 0) return false;
    const uint64_t alignment = (1024u * 1024u + sector_size - 1) / sector_size;
    uint64_t disk_sectors = disk->size_bytes / sector_size;
    uint64_t range_start = alignment;
    uint64_t range_end = disk_sectors;
    if (disk->style == PARTITION_STYLE_GPT) {
        if (disk_sectors <= 34) return false;
        disk_sectors -= 34;
        range_end = disk_sectors;
        if (disk->initialize_layout) {
            range_start = (17u * 1024u * 1024u + sector_size - 1) / sector_size;
        }
    } else if (logical) {
        const size_t extended_index = FindExtendedPartition(disk);
        if (extended_index == SIZE_MAX) return false;
        const FdiskPartition* extended = &disk->partitions[extended_index];
        range_start = extended->start_sector + alignment;
        range_end = extended->start_sector + extended->sector_count;
    }

    uint64_t cursor = AlignUp(range_start, alignment);
    while (cursor < range_end) {
        uint64_t nearest_start = UINT64_MAX;
        uint64_t nearest_end = UINT64_MAX;
        for (size_t i = 0; i < disk->partition_count; ++i) {
            const FdiskPartition* partition = &disk->partitions[i];
            if (logical) {
                if (!IsLogicalPartition(partition)) continue;
            } else if (disk->style == PARTITION_STYLE_MBR && IsLogicalPartition(partition)) {
                continue;
            }
            const uint64_t start = partition->start_sector;
            const uint64_t end = start + partition->sector_count;
            if (end <= cursor) continue;
            if (start < nearest_start) {
                nearest_start = start;
                nearest_end = end;
            }
        }

        const uint64_t limit = nearest_start < range_end ? nearest_start : range_end;
        if (cursor <= limit && needed_sectors <= limit - cursor) {
            *start_sector = cursor;
            return true;
        }
        if (nearest_start == UINT64_MAX) return false;
        cursor = AlignUp(nearest_end, alignment);
        if (cursor == UINT64_MAX) return false;
    }
    return false;
}

static size_t FindPartitionIndex(const FdiskDisk* disk, uint32_t number) {
    for (size_t i = 0; i < disk->partition_count; ++i) {
        if (disk->partitions[i].number == number) return i;
    }
    return SIZE_MAX;
}

static void SortAndNumberPartitions(FdiskDisk* disk) {
    for (size_t i = 1; i < disk->partition_count; ++i) {
        FdiskPartition current = disk->partitions[i];
        size_t position = i;
        while (position > 0 &&
            disk->partitions[position - 1].start_sector > current.start_sector) {
            disk->partitions[position] = disk->partitions[position - 1];
            --position;
        }
        disk->partitions[position] = current;
    }
    uint32_t primary_number = 1;
    uint32_t logical_number = 5;
    for (size_t i = 0; i < disk->partition_count; ++i) {
        FdiskPartition* partition = &disk->partitions[i];
        const bool logical = disk->style == PARTITION_STYLE_MBR &&
            partition->native.PartitionNumber >= 5;
        partition->number = logical ? logical_number++ : primary_number++;
        partition->native.PartitionNumber = partition->number;
    }
}

static void RemovePartition(FdiskDisk* disk, size_t index) {
    for (size_t i = index + 1; i < disk->partition_count; ++i) {
        disk->partitions[i - 1] = disk->partitions[i];
    }
    --disk->partition_count;
    SortAndNumberPartitions(disk);
}

static bool CreatePartition(FdiskDisk* disk) {
    if (disk->partition_count >= 128) {
        fwprintf(stderr, L"fdisk: the supported partition entry limit has been reached.\n");
        return false;
    }
    bool logical = false;
    bool extended = false;
    if (disk->style == PARTITION_STYLE_MBR) {
        wchar_t kind[32];
        if (!ReadLine(L"Partition kind (primary, extended, logical): ", kind, _countof(kind))) return false;
        if (_wcsicmp(kind, L"logical") == 0) logical = true;
        else if (_wcsicmp(kind, L"extended") == 0) extended = true;
        else if (_wcsicmp(kind, L"primary") != 0) {
            fwprintf(stderr, L"fdisk: choose primary, extended, or logical.\n");
            return false;
        }
        if (!logical && CountPrimaryMbrPartitions(disk) >= 4) {
            fwprintf(stderr, L"fdisk: MBR supports at most four primary/extended partitions.\n");
            return false;
        }
        if (extended && FindExtendedPartition(disk) != SIZE_MAX) {
            fwprintf(stderr, L"fdisk: an extended partition already exists.\n");
            return false;
        }
        if (logical && FindExtendedPartition(disk) == SIZE_MAX) {
            fwprintf(stderr, L"fdisk: create an extended partition before adding logical partitions.\n");
            return false;
        }
    }
    if (disk->style == PARTITION_STYLE_GPT &&
        disk->partition_count >= (disk->gpt_max_partition_count != 0
            ? disk->gpt_max_partition_count : 128)) {
        fwprintf(stderr, L"fdisk: the GPT partition entry limit has been reached.\n");
        return false;
    }

    const uint64_t max_mib = disk->size_bytes / (1024u * 1024u);
    uint64_t size_mib = 0;
    if (!ReadNumber(L"Size in MiB: ", 1, max_mib, &size_mib)) return false;
    if (size_mib > UINT64_MAX / (1024u * 1024u)) {
        fwprintf(stderr, L"fdisk: requested size is too large.\n");
        return false;
    }
    const uint64_t size_bytes = size_mib * 1024u * 1024u;
    if (size_bytes % disk->sector_size != 0) {
        fwprintf(stderr, L"fdisk: requested size is not sector-aligned.\n");
        return false;
    }
    const uint64_t sectors = size_bytes / disk->sector_size;
    uint64_t start_sector = 0;
    if (!FindFreeExtent(disk, sectors, logical, &start_sector)) {
        fwprintf(stderr, L"fdisk: no aligned free extent is large enough in the selected area.\n");
        return false;
    }
    if (disk->style == PARTITION_STYLE_MBR &&
        (start_sector > UINT32_MAX || sectors > UINT32_MAX ||
            start_sector > (uint64_t)UINT32_MAX + 1 - sectors)) {
        fwprintf(stderr, L"fdisk: MBR partition extent exceeds the 32-bit sector limit.\n");
        return false;
    }

    FdiskPartition* resized = (FdiskPartition*)realloc(
        disk->partitions, (disk->partition_count + 1) * sizeof(*disk->partitions));
    if (resized == NULL) {
        fwprintf(stderr, L"fdisk: out of memory creating partition.\n");
        return false;
    }
    disk->partitions = resized;
    FdiskPartition* partition = &disk->partitions[disk->partition_count];
    memset(partition, 0, sizeof(*partition));
    partition->number = (uint32_t)(disk->partition_count + 1);
    partition->start_sector = start_sector;
    partition->sector_count = sectors;
    partition->size_bytes = size_bytes;
    partition->native.PartitionStyle = disk->style;
    partition->native.PartitionNumber = partition->number;
    partition->native.StartingOffset.QuadPart = (LONGLONG)(start_sector * disk->sector_size);
    partition->native.PartitionLength.QuadPart = (LONGLONG)size_bytes;
    partition->native.RewritePartition = TRUE;

    if (disk->style == PARTITION_STYLE_MBR) {
        const size_t primary_count = CountPrimaryMbrPartitions(disk);
        uint32_t logical_number = 5;
        for (size_t i = 0; i < disk->partition_count; ++i) {
            if (IsLogicalPartition(&disk->partitions[i]) &&
                disk->partitions[i].number >= logical_number) {
                logical_number = disk->partitions[i].number + 1;
            }
        }
        partition->number = logical ? logical_number : (uint32_t)(primary_count + 1);
        partition->native.PartitionNumber = partition->number;
        partition->mbr_type = extended ? 0x0f : 0x07;
        partition->native.Mbr.PartitionType = partition->mbr_type;
        partition->native.Mbr.RecognizedPartition = !extended;
        partition->native.Mbr.HiddenSectors = (DWORD)start_sector;
    } else {
        partition->gpt_type = GPT_BASIC_DATA;
        partition->native.Gpt.PartitionType = GPT_BASIC_DATA;
        if (CoCreateGuid(&partition->native.Gpt.PartitionId) != S_OK) {
            fwprintf(stderr, L"fdisk: could not generate a unique GPT partition ID.\n");
            return false;
        }
        if (wcscpy_s(partition->native.Gpt.Name, _countof(partition->native.Gpt.Name),
                L"New Partition") != 0) {
            fwprintf(stderr, L"fdisk: could not set the GPT partition name.\n");
            return false;
        }
        partition->gpt_id = partition->native.Gpt.PartitionId;
        wcscpy_s(partition->name, _countof(partition->name), L"New Partition");
    }
    ++disk->partition_count;
    SortAndNumberPartitions(disk);
    wprintf(L"Partition staged at sector %llu (%llu sectors).\n",
        (unsigned long long)start_sector, (unsigned long long)sectors);
    return true;
}

static bool ChangePartitionType(FdiskDisk* disk) {
    uint64_t number = 0;
    if (!ReadNumber(L"Partition number: ", 1, UINT32_MAX, &number)) return false;
    const size_t index = FindPartitionIndex(disk, (uint32_t)number);
    if (index == SIZE_MAX) {
        fwprintf(stderr, L"fdisk: partition not found.\n");
        return false;
    }

    wchar_t value[64];
    if (disk->style == PARTITION_STYLE_MBR) {
        if (!ReadLine(L"MBR type byte (hex, e.g. 07): ", value, _countof(value))) return false;
        wchar_t* end = NULL;
        errno = 0;
        const unsigned long type = wcstoul(value, &end, 16);
        if (errno != 0 || end == value || *end != L'\0' || type == 0 || type > 0xff) {
            fwprintf(stderr, L"fdisk: invalid MBR type byte.\n");
            return false;
        }
        const bool current_extended = IsExtendedPartition(&disk->partitions[index]);
        const bool new_extended = type == 0x05 || type == 0x0f;
        if (new_extended && !current_extended && FindExtendedPartition(disk) != SIZE_MAX) {
            fwprintf(stderr, L"fdisk: an extended partition already exists.\n");
            return false;
        }
        if (new_extended && IsLogicalPartition(&disk->partitions[index])) {
            fwprintf(stderr, L"fdisk: a logical partition cannot become an extended container.\n");
            return false;
        }
        if (current_extended && !new_extended) {
            const uint64_t extended_start = disk->partitions[index].start_sector;
            const uint64_t extended_end = extended_start + disk->partitions[index].sector_count;
            for (size_t i = 0; i < disk->partition_count; ++i) {
                if (IsLogicalPartition(&disk->partitions[i]) &&
                    disk->partitions[i].start_sector >= extended_start &&
                    disk->partitions[i].start_sector < extended_end) {
                    fwprintf(stderr, L"fdisk: delete logical partitions before changing their extended container type.\n");
                    return false;
                }
            }
        }
        disk->partitions[index].mbr_type = (uint8_t)type;
        disk->partitions[index].native.Mbr.PartitionType = (BYTE)type;
        disk->partitions[index].native.Mbr.RecognizedPartition = !new_extended;
    } else {
        if (!ReadLine(L"GPT type (basic, efi, msr, recovery): ", value, _countof(value))) return false;
        const GUID* type = NULL;
        if (_wcsicmp(value, L"basic") == 0) type = &GPT_BASIC_DATA;
        else if (_wcsicmp(value, L"efi") == 0) type = &GPT_EFI_SYSTEM;
        else if (_wcsicmp(value, L"msr") == 0) type = &GPT_MICROSOFT_RESERVED;
        else if (_wcsicmp(value, L"recovery") == 0) type = &GPT_WINDOWS_RECOVERY;
        if (type == NULL) {
            fwprintf(stderr, L"fdisk: unsupported GPT type.\n");
            return false;
        }
        disk->partitions[index].gpt_type = *type;
        disk->partitions[index].native.Gpt.PartitionType = *type;
    }
    wprintf(L"Partition type staged.\n");
    return true;
}

static bool ResizePartition(FdiskDisk* disk) {
    uint64_t number = 0;
    if (!ReadNumber(L"Partition number: ", 1, UINT32_MAX, &number)) return false;
    const size_t index = FindPartitionIndex(disk, (uint32_t)number);
    if (index == SIZE_MAX) {
        fwprintf(stderr, L"fdisk: partition not found.\n");
        return false;
    }

    uint64_t size_mib = 0;
    if (!ReadNumber(L"New size in MiB: ", 1, disk->size_bytes / (1024u * 1024u), &size_mib)) return false;
    if (size_mib > UINT64_MAX / (1024u * 1024u)) return false;
    const uint64_t size_bytes = size_mib * 1024u * 1024u;
    if (size_bytes % disk->sector_size != 0) {
        fwprintf(stderr, L"fdisk: requested size is not sector-aligned.\n");
        return false;
    }
    const uint64_t sectors = size_bytes / disk->sector_size;
    FdiskPartition* partition = &disk->partitions[index];
    uint64_t disk_sectors = disk->size_bytes / disk->sector_size;
    if (disk->style == PARTITION_STYLE_GPT) {
        if (disk_sectors <= 34) {
            fwprintf(stderr, L"fdisk: disk is too small for a GPT partition.\n");
            return false;
        }
        disk_sectors -= 34;
    }
    const bool logical = IsLogicalPartition(partition);
    if (logical) {
        const size_t extended_index = FindExtendedPartition(disk);
        if (extended_index == SIZE_MAX) {
            fwprintf(stderr, L"fdisk: logical partition has no extended container.\n");
            return false;
        }
        const FdiskPartition* extended = &disk->partitions[extended_index];
        disk_sectors = extended->start_sector + extended->sector_count;
    }
    const uint64_t end = partition->start_sector + sectors;
    if (end < partition->start_sector || end > disk_sectors) {
        fwprintf(stderr, L"fdisk: requested extent exceeds disk boundaries.\n");
        return false;
    }
    if (disk->style == PARTITION_STYLE_MBR &&
        (partition->start_sector > UINT32_MAX || sectors > UINT32_MAX ||
            partition->start_sector > (uint64_t)UINT32_MAX + 1 - sectors)) {
        fwprintf(stderr, L"fdisk: MBR partition extent exceeds the 32-bit sector limit.\n");
        return false;
    }
    for (size_t i = 0; i < disk->partition_count; ++i) {
        if (i == index) continue;
        if (logical && IsExtendedPartition(&disk->partitions[i])) continue;
        const uint64_t other_start = disk->partitions[i].start_sector;
        const uint64_t other_end = other_start + disk->partitions[i].sector_count;
        if (partition->start_sector < other_end && other_start < end) {
            fwprintf(stderr, L"fdisk: requested extent overlaps another partition.\n");
            return false;
        }
    }
    partition->sector_count = sectors;
    partition->size_bytes = size_bytes;
    partition->native.PartitionLength.QuadPart = (LONGLONG)size_bytes;
    partition->native.RewritePartition = TRUE;
    wprintf(L"Partition size staged. Shrinking can make data beyond the new end inaccessible.\n");
    return true;
}

static bool InitializeLayout(FdiskDisk* disk, PARTITION_STYLE style) {
    wchar_t confirmation[32];
    const wchar_t* expected = style == PARTITION_STYLE_GPT ? L"GPT" : L"MBR";
    if (!ReadLine(L"This replaces the current partition table. Type GPT or MBR to stage it: ",
            confirmation, _countof(confirmation)) || _wcsicmp(confirmation, expected) != 0) {
        fwprintf(stderr, L"fdisk: initialization cancelled.\n");
        return false;
    }

    GUID disk_id = {0};
    if (style == PARTITION_STYLE_MBR) {
        if (CoCreateGuid(&disk_id) != S_OK || disk_id.Data1 == 0) {
            fwprintf(stderr, L"fdisk: could not generate an MBR signature.\n");
            return false;
        }
    } else {
        if (CoCreateGuid(&disk_id) != S_OK) {
            fwprintf(stderr, L"fdisk: could not generate a GPT disk ID.\n");
            return false;
        }
    }

    free(disk->partitions);
    disk->partitions = NULL;
    disk->partition_count = 0;
    disk->style = style;
    disk->initialize_layout = true;
    if (style == PARTITION_STYLE_MBR) disk->mbr_signature = disk_id.Data1;
    else {
        disk->gpt_id = disk_id;
        disk->gpt_max_partition_count = 128;
    }
    wprintf(L"New %ls partition table staged; it will not be written unless you confirm with 'w'.\n",
        expected);
    return true;
}

static bool ToggleBootable(FdiskDisk* disk) {
    if (disk->style != PARTITION_STYLE_MBR) {
        fwprintf(stderr, L"fdisk: the active/bootable flag is only available for MBR.\n");
        return false;
    }
    uint64_t number = 0;
    if (!ReadNumber(L"Partition number: ", 1, UINT32_MAX, &number)) return false;
    const size_t index = FindPartitionIndex(disk, (uint32_t)number);
    if (index == SIZE_MAX) {
        fwprintf(stderr, L"fdisk: partition not found.\n");
        return false;
    }
    if (IsLogicalPartition(&disk->partitions[index]) ||
        IsExtendedPartition(&disk->partitions[index])) {
        fwprintf(stderr, L"fdisk: only primary MBR partitions can be marked active.\n");
        return false;
    }
    const bool enable = !disk->partitions[index].bootable;
    if (enable) {
        for (size_t i = 0; i < disk->partition_count; ++i) {
            disk->partitions[i].bootable = false;
            disk->partitions[i].native.Mbr.BootIndicator = FALSE;
        }
    }
    disk->partitions[index].bootable = enable;
    disk->partitions[index].native.Mbr.BootIndicator = disk->partitions[index].bootable;
    disk->partitions[index].native.RewritePartition = TRUE;
    wprintf(L"Bootable flag staged.\n");
    return true;
}

static int CommitLayout(FdiskDisk* disk, bool sector_units) {
    wchar_t confirmation[64];
    wprintf(L"WARNING: writing this layout changes PhysicalDrive%lu and can make data inaccessible.\n",
        (unsigned long)disk->number);
    if (!ReadLine(L"Type WRITE to apply the staged partition table: ",
            confirmation, _countof(confirmation)) || wcscmp(confirmation, L"WRITE") != 0) {
        fwprintf(stderr, L"fdisk: write cancelled; no staged changes were applied.\n");
        return 2;
    }

    FdiskError error = {0};
    if (!FdiskWriteDiskLayout(disk, &error)) {
        fwprintf(stderr, L"fdisk: %ls\n", error.message);
        return error.code == ERROR_ACCESS_DENIED ? 5 : 1;
    }
    wprintf(L"Partition table written successfully. Final layout:\n");
    if (!FdiskPrintDisk(disk, sector_units, &error)) {
        fwprintf(stderr, L"fdisk: %ls\n", error.message);
        return 1;
    }
    return 0;
}

int FdiskEditDisk(uint32_t number, bool sector_units) {
    FdiskDisk disk = {0};
    FdiskError error = {0};
    if (!FdiskReadDisk(number, &disk, &error)) {
        fwprintf(stderr, L"fdisk: PhysicalDrive%lu: %ls\n", (unsigned long)number, error.message);
        return error.code == ERROR_ACCESS_DENIED ? 5 : 1;
    }

    wprintf(L"\nfdisk interactive editor for PhysicalDrive%lu\n"
        L"Changes are staged in memory. 'w' is required to write them; 'q' discards them.\n"
        L"Commands: p print, n new, d delete, t type, a active, s resize, g GPT, o MBR, w write, q quit\n",
        (unsigned long)number);

    int result = 0;
    wchar_t line[64];
    for (;;) {
        if (!FdiskPrintDisk(&disk, sector_units, &error)) {
            fwprintf(stderr, L"fdisk: %ls\n", error.message);
            result = 1;
            break;
        }
        if (!ReadLine(L"fdisk> ", line, _countof(line))) break;
        if (line[0] == L'\0') continue;

        switch (line[0]) {
            case L'p':
            case L'P':
                break;
            case L'n':
            case L'N':
                (void)CreatePartition(&disk);
                break;
            case L'd':
            case L'D': {
                uint64_t number_to_delete = 0;
                if (ReadNumber(L"Partition number to delete: ", 1, UINT32_MAX, &number_to_delete)) {
                    const size_t index = FindPartitionIndex(&disk, (uint32_t)number_to_delete);
                    if (index == SIZE_MAX) fwprintf(stderr, L"fdisk: partition not found.\n");
                    else if (IsExtendedPartition(&disk.partitions[index])) {
                        bool has_logical = false;
                        const uint64_t extended_start = disk.partitions[index].start_sector;
                        const uint64_t extended_end = extended_start +
                            disk.partitions[index].sector_count;
                        for (size_t i = 0; i < disk.partition_count; ++i) {
                            if (IsLogicalPartition(&disk.partitions[i]) &&
                                disk.partitions[i].start_sector >= extended_start &&
                                disk.partitions[i].start_sector < extended_end) {
                                has_logical = true;
                                break;
                            }
                        }
                        if (has_logical) {
                            fwprintf(stderr, L"fdisk: delete logical partitions before deleting their extended container.\n");
                        } else {
                            RemovePartition(&disk, index);
                            wprintf(L"Partition deletion staged.\n");
                        }
                    }
                    else {
                        RemovePartition(&disk, index);
                        wprintf(L"Partition deletion staged.\n");
                    }
                }
                break;
            }
            case L't':
            case L'T':
                (void)ChangePartitionType(&disk);
                break;
            case L'a':
            case L'A':
                (void)ToggleBootable(&disk);
                break;
            case L's':
            case L'S':
                (void)ResizePartition(&disk);
                break;
            case L'g':
            case L'G':
                (void)InitializeLayout(&disk, PARTITION_STYLE_GPT);
                break;
            case L'o':
            case L'O':
                (void)InitializeLayout(&disk, PARTITION_STYLE_MBR);
                break;
            case L'w':
            case L'W':
                result = CommitLayout(&disk, sector_units);
                goto done;
            case L'q':
            case L'Q':
                goto done;
            default:
                fwprintf(stderr, L"fdisk: unknown command; use p, n, d, t, a, s, g, o, w, or q.\n");
                break;
        }
    }

done:
    FdiskFreeDisk(&disk);
    return result;
}
