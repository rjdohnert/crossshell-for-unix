#include "fdisk_format.h"

#include "fdisk_error.h"

#include <stdio.h>
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
static const GUID GPT_LINUX_FILESYSTEM = {
    0x0fc63daf, 0x8483, 0x4772, {0x8e, 0x79, 0x3d, 0x69, 0xd8, 0x47, 0x7d, 0xe4}
};

static const wchar_t* GetPartitionType(const FdiskDisk* disk, const FdiskPartition* partition) {
    if (disk->style == PARTITION_STYLE_MBR) {
        switch (partition->mbr_type) {
            case 0x01: return L"FAT12";
            case 0x04:
            case 0x06:
            case 0x0e: return L"FAT16";
            case 0x07: return L"NTFS/exFAT";
            case 0x0b:
            case 0x0c: return L"FAT32";
            case 0x05:
            case 0x0f: return L"Extended";
            case 0x82: return L"Linux swap";
            case 0x83: return L"Linux";
            case 0xee: return L"GPT protective";
            default: return L"Unknown";
        }
    }
    if (disk->style == PARTITION_STYLE_GPT) {
        if (IsEqualGUID(&partition->gpt_type, &GPT_BASIC_DATA)) return L"Microsoft basic data";
        if (IsEqualGUID(&partition->gpt_type, &GPT_EFI_SYSTEM)) return L"EFI system";
        if (IsEqualGUID(&partition->gpt_type, &GPT_MICROSOFT_RESERVED)) return L"Microsoft reserved";
        if (IsEqualGUID(&partition->gpt_type, &GPT_WINDOWS_RECOVERY)) return L"Windows recovery";
        if (IsEqualGUID(&partition->gpt_type, &GPT_LINUX_FILESYSTEM)) return L"Linux filesystem";
        return L"GPT";
    }
    return L"Unpartitioned";
}

static void FormatSize(uint64_t bytes, wchar_t* output, size_t output_count) {
    static const wchar_t* units[] = {L"B", L"KiB", L"MiB", L"GiB", L"TiB", L"PiB"};
    double value = (double)bytes;
    size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < _countof(units)) {
        value /= 1024.0;
        ++unit;
    }
    (void)swprintf_s(output, output_count, L"%.2f %ls", value, units[unit]);
}

bool FdiskPrintDisk(const FdiskDisk* disk, bool sector_units, FdiskError* error) {
    wchar_t total[64];
    FormatSize(disk->size_bytes, total, _countof(total));

    const wchar_t* style = L"RAW";
    if (disk->style == PARTITION_STYLE_MBR) style = L"MBR";
    if (disk->style == PARTITION_STYLE_GPT) style = L"GPT";

    const uint64_t sectors_per_cylinder =
        (uint64_t)disk->sectors_per_track * disk->tracks_per_cylinder;
    const bool show_cylinders = !sector_units && sectors_per_cylinder != 0;
    if (wprintf(L"\nDisk %lu (%ls): %ls, %llu bytes, %lu-byte sectors, %ls\n",
            (unsigned long)disk->number, disk->device_path, total,
            (unsigned long long)disk->size_bytes, (unsigned long)disk->sector_size, style) < 0 ||
        wprintf(show_cylinders
            ? L"  No.  Start cyl      End cyl        Size  Boot  Type\n"
            : L"  No.  Start sector     Sectors          Size  Boot  Type\n") < 0) {
        FdiskErrorSet(error, ERROR_WRITE_FAULT, L"Failed writing disk information");
        return false;
    }
    if (!sector_units && !show_cylinders &&
        wprintf(L"  CHS geometry unavailable; displaying sectors.\n") < 0) {
        FdiskErrorSet(error, ERROR_WRITE_FAULT, L"Failed writing disk information");
        return false;
    }

    for (size_t i = 0; i < disk->partition_count; ++i) {
        const FdiskPartition* partition = &disk->partitions[i];
        wchar_t size[64];
        FormatSize(partition->size_bytes, size, _countof(size));
        int written;
        if (show_cylinders) {
            const uint64_t start_cylinder = partition->start_sector / sectors_per_cylinder;
            const uint64_t end_sector = partition->start_sector + partition->sector_count - 1;
            const uint64_t end_cylinder = end_sector / sectors_per_cylinder;
            written = wprintf(L"  %3lu  %12llu  %12llu  %12ls  %ls  %ls%ls%ls\n",
                (unsigned long)partition->number,
                (unsigned long long)start_cylinder,
                (unsigned long long)end_cylinder,
                size,
                partition->bootable ? L"yes" : L"no ",
                GetPartitionType(disk, partition),
                partition->name[0] != L'\0' ? L" - " : L"",
                partition->name[0] != L'\0' ? partition->name : L"");
        } else {
            written = wprintf(L"  %3lu  %12llu  %12llu  %12ls  %ls  %ls%ls%ls\n",
                (unsigned long)partition->number,
                (unsigned long long)partition->start_sector,
                (unsigned long long)partition->sector_count,
                size,
                partition->bootable ? L"yes" : L"no ",
                GetPartitionType(disk, partition),
                partition->name[0] != L'\0' ? L" - " : L"",
                partition->name[0] != L'\0' ? partition->name : L"");
        }
        if (written < 0) {
            FdiskErrorSet(error, ERROR_WRITE_FAULT, L"Failed writing partition information");
            return false;
        }
    }

    return true;
}

void FdiskPrintUsage(void) {
    wprintf(
        L"Usage:\n"
        L"  fdisk <disk>       Edit a disk's partition table interactively\n"
        L"  fdisk -l [<disk>]\n\n"
        L"  fdisk --image <path> [--list] [-u]\n"
        L"  fdisk --create-image <path.vhd|path.vhdx> --size <MiB> [--format vhd|vhdx]\n"
        L"  fdisk --resize-image <path.vhd|path.vhdx> --size <MiB>\n"
        L"  fdisk --detach-image <path.vhd|path.vhdx>\n\n"
        L"Partition-table editor and disk information utility for Windows.\n"
        L"Edits are staged; use 'w' and type WRITE to apply them to disk.\n\n"
        L"Options:\n"
        L"  -l, --list       List partitions on all disks, or the specified disk\n"
        L"  -u, --sectors    Display partition locations in sectors\n"
        L"      --image <path>       Attach an image and edit it; it stays attached on exit\n"
        L"      --create-image <path> Create a dynamic VHD or VHDX image\n"
        L"      --resize-image <path> Safely resize a detached image\n"
        L"      --detach-image <path> Detach an attached image\n"
        L"      --size       Image capacity in MiB for create or resize operations\n"
        L"      --format     Creation format (vhd or vhdx; inferred from extension by default)\n"
        L"  -h, --help       Display this help and exit\n"
        L"  -V, --version    Display version information and exit\n\n"
        L"Disk targets may be a decimal index, PhysicalDriveN, or \\\\.\\PhysicalDriveN.\n"
        L"Examples:\n"
        L"  fdisk -l\n"
        L"  fdisk -l 0\n"
        L"  fdisk -u -l 0\n"
        L"  fdisk 1\n"
        L"  fdisk \\\\.\\PhysicalDrive1\n"
        L"  fdisk --create-image C:\\disks\\lab.vhdx --size 4096\n"
        L"  fdisk --image C:\\disks\\lab.vhdx\n"
        L"  fdisk --resize-image C:\\disks\\lab.vhdx --size 8192\n"
        L"  fdisk --detach-image C:\\disks\\lab.vhdx\n\n"
        L"Editor commands: p print, n new, d delete, t type, a active, s resize,\n"
        L"  g initialize GPT, o initialize MBR, w write, q quit without writing.\n");
}

void FdiskPrintVersion(void) {
    wprintf(L"fdisk %ls (Windows partition-table editor)\n", FDISK_VERSION);
}
