#include "collection.h"

static void VolumeCollection_Add(VolumeCollection* self, IVolume* vol) {
    if (!vol) return;
    if (self->count >= self->capacity) {
        size_t new_cap = (self->capacity == 0) ? 16 : self->capacity * 2;
        IVolume** new_items = (IVolume**)realloc(self->items, new_cap * sizeof(IVolume*));
        if (!new_items) return;
        self->items = new_items;
        self->capacity = new_cap;
    }
    self->items[self->count++] = vol;
}

static size_t VolumeCollection_Count(const VolumeCollection* self) {
    return self->count;
}

static IVolume* VolumeCollection_GetAt(const VolumeCollection* self, size_t index) {
    return (index < self->count) ? self->items[index] : NULL;
}

static bool VolumeCollection_ContainsMount(const VolumeCollection* self, const char* mount) {
    if (!mount) return false;
    for (size_t i = 0; i < self->count; ++i) {
        const char* m = self->items[i]->vptr->GetMountPoint(self->items[i]);
        if (_stricmp(m, mount) == 0) return true;
    }
    return false;
}

static void VolumeCollection_Destroy(VolumeCollection* self) {
    if (self) {
        for (size_t i = 0; i < self->count; ++i) {
            self->items[i]->vptr->Destroy(self->items[i]);
        }
        free(self->items);
        free(self);
    }
}

static const VolumeCollectionVTable g_VolumeCollectionVTable = {
    VolumeCollection_Add,
    VolumeCollection_Count,
    VolumeCollection_GetAt,
    VolumeCollection_ContainsMount,
    VolumeCollection_Destroy
};

VolumeCollection* VolumeCollection_Create(void) {
    VolumeCollection* vc = (VolumeCollection*)calloc(1, sizeof(VolumeCollection));
    if (vc) vc->vptr = &g_VolumeCollectionVTable;
    return vc;
}
