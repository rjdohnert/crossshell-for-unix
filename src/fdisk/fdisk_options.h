#ifndef FDISK_OPTIONS_H
#define FDISK_OPTIONS_H

#include "fdisk_types.h"

typedef enum FdiskImageAction {
    FDISK_IMAGE_ACTION_NONE,
    FDISK_IMAGE_ACTION_USE,
    FDISK_IMAGE_ACTION_CREATE,
    FDISK_IMAGE_ACTION_RESIZE,
    FDISK_IMAGE_ACTION_DETACH
} FdiskImageAction;

typedef struct FdiskOptions {
    bool list_all;
    bool sector_units;
    bool show_help;
    bool show_version;
    bool has_disk;
    uint32_t disk_number;
    FdiskImageAction image_action;
    const wchar_t* image_path;
    uint64_t image_size_bytes;
    FdiskImageFormat image_format;
    bool has_image_size;
    bool image_format_explicit;
} FdiskOptions;

bool FdiskParseOptions(int argc, wchar_t* argv[], FdiskOptions* options, FdiskError* error);

#endif
