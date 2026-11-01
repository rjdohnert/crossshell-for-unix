#ifndef VOLUME_H
#define VOLUME_H

#include "common.h"

typedef struct IVolume IVolume;

typedef struct IVolumeVTable {
    const char* (*GetDeviceName)(const IVolume* self);
    const char* (*GetMountPoint)(const IVolume* self);
    const char* (*GetFSType)(const IVolume* self);
    bool        (*IsLocal)(const IVolume* self);
    uint64_t    (*GetTotalKbytes)(const IVolume* self);
    uint64_t    (*GetUsedKbytes)(const IVolume* self);
    uint64_t    (*GetAvailKbytes)(const IVolume* self);
    int         (*GetPercentUsed)(const IVolume* self);
    uint64_t    (*GetUsedInodes)(const IVolume* self);
    uint64_t    (*GetFreeInodes)(const IVolume* self);
    int         (*GetPercentInodesUsed)(const IVolume* self);
    bool        (*Sync)(IVolume* self);
    void        (*Destroy)(IVolume* self);
} IVolumeVTable;

struct IVolume {
    const IVolumeVTable* vptr;
};

IVolume* DiskVolume_Create(const char* device, const char* mount, const char* fstype,
                           const wchar_t* guid, bool local, uint64_t total, uint64_t used,
                           uint64_t avail, uint64_t iused, uint64_t ifree);
IVolume* SwapVolume_Create(const char* device, const char* mount,
                           uint64_t total, uint64_t used, uint64_t avail);

#endif
