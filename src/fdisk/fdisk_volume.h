#ifndef FDISK_VOLUME_H
#define FDISK_VOLUME_H

#include "fdisk_types.h"

typedef struct FdiskLockedVolume {
    HANDLE handle;
    bool locked;
} FdiskLockedVolume;

typedef struct FdiskVolumeLocks {
    FdiskLockedVolume* items;
    size_t count;
} FdiskVolumeLocks;

bool FdiskLockDiskVolumes(uint32_t disk_number, FdiskVolumeLocks* locks, FdiskError* error);
bool FdiskUnlockDiskVolumes(FdiskVolumeLocks* locks, FdiskError* error);

#endif
