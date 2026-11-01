#ifndef COLLECTION_H
#define COLLECTION_H

#include "common.h"
#include "volume.h"

typedef struct VolumeCollection VolumeCollection;

typedef struct VolumeCollectionVTable {
    void     (*Add)(VolumeCollection* self, IVolume* vol);
    size_t   (*Count)(const VolumeCollection* self);
    IVolume* (*GetAt)(const VolumeCollection* self, size_t index);
    bool     (*ContainsMount)(const VolumeCollection* self, const char* mount);
    void     (*Destroy)(VolumeCollection* self);
} VolumeCollectionVTable;

struct VolumeCollection {
    const VolumeCollectionVTable* vptr;
    IVolume** items;
    size_t    count;
    size_t    capacity;
};

VolumeCollection* VolumeCollection_Create(void);

#endif
