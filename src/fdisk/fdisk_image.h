#ifndef FDISK_IMAGE_H
#define FDISK_IMAGE_H

#include "fdisk_types.h"

typedef struct FdiskImageSession {
    HANDLE handle;
    uint32_t disk_number;
} FdiskImageSession;

bool FdiskImageCreate(const wchar_t* path, FdiskImageFormat format, uint64_t size_bytes,
    FdiskError* error);
bool FdiskImageResize(const wchar_t* path, uint64_t size_bytes, FdiskError* error);
bool FdiskImageAttach(const wchar_t* path, FdiskImageSession* session, FdiskError* error);
bool FdiskImageDetach(const wchar_t* path, FdiskError* error);
bool FdiskImageCloseSession(FdiskImageSession* session, FdiskError* error);

#endif
