#ifndef FDISK_DEVICE_H
#define FDISK_DEVICE_H

#include "fdisk_types.h"

bool FdiskEnumerateDiskNumbers(uint32_t** numbers, size_t* count, FdiskError* error);
bool FdiskReadDisk(uint32_t number, FdiskDisk* disk, FdiskError* error);
bool FdiskWriteDiskLayout(FdiskDisk* disk, FdiskError* error);
void FdiskFreeDisk(FdiskDisk* disk);

#endif
