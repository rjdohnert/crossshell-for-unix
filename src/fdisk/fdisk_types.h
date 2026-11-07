#ifndef FDISK_TYPES_H
#define FDISK_TYPES_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <windows.h>
#include <winioctl.h>

#define FDISK_VERSION L"2.39"
#define FDISK_ERROR_CAPACITY 512

typedef enum FdiskImageFormat {
    FDISK_IMAGE_FORMAT_VHDX,
    FDISK_IMAGE_FORMAT_VHD
} FdiskImageFormat;

typedef struct FdiskError {
    DWORD code;
    wchar_t message[FDISK_ERROR_CAPACITY];
} FdiskError;

typedef struct FdiskPartition {
    uint32_t number;
    bool bootable;
    uint64_t start_sector;
    uint64_t sector_count;
    uint64_t size_bytes;
    uint8_t mbr_type;
    GUID gpt_type;
    GUID gpt_id;
    wchar_t name[37];
    PARTITION_INFORMATION_EX native;
} FdiskPartition;

typedef struct FdiskDisk {
    uint32_t number;
    wchar_t device_path[64];
    uint64_t size_bytes;
    uint32_t sector_size;
    uint32_t sectors_per_track;
    uint32_t tracks_per_cylinder;
    PARTITION_STYLE style;
    uint32_t mbr_signature;
    GUID gpt_id;
    uint32_t gpt_max_partition_count;
    bool initialize_layout;
    FdiskPartition* partitions;
    size_t partition_count;
} FdiskDisk;

#endif
