#ifndef MKFS_TYPES_H
#define MKFS_TYPES_H

#include <stdbool.h>
#include <windows.h>

#define MKFS_ERROR_CAPACITY 512
#define MKFS_LABEL_CAPACITY 33

typedef enum MkfsFileSystem {
    MKFS_FILESYSTEM_NTFS,
    MKFS_FILESYSTEM_FAT32,
    MKFS_FILESYSTEM_EXFAT
} MkfsFileSystem;

typedef struct MkfsOptions {
    wchar_t drive_letter;
    MkfsFileSystem file_system;
    wchar_t label[MKFS_LABEL_CAPACITY];
    DWORD cluster_size;
    bool has_label;
    bool quick_format;
    bool verbose;
    bool assume_yes;
    bool show_help;
    bool show_version;
} MkfsOptions;

typedef struct MkfsError {
    DWORD code;
    wchar_t message[MKFS_ERROR_CAPACITY];
} MkfsError;

const wchar_t* MkfsFileSystemName(MkfsFileSystem file_system);

#endif
