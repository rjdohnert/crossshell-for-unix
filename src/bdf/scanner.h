#ifndef SCANNER_H
#define SCANNER_H

#include "common.h"
#include "collection.h"

typedef struct IVolumeScanner IVolumeScanner;

typedef struct IVolumeScannerVTable {
    VolumeCollection* (*ScanAllVolumes)(IVolumeScanner* self, bool local_only, const char* fstype);
    VolumeCollection* (*ScanPathVolumes)(IVolumeScanner* self, int path_count, char** paths,
                                        bool local_only, const char* fstype, int* out_failed);
    VolumeCollection* (*ScanSwapVolumes)(IVolumeScanner* self, const char* fstype);
    void              (*Destroy)(IVolumeScanner* self);
} IVolumeScannerVTable;

struct IVolumeScanner {
    const IVolumeScannerVTable* vptr;
};

IVolumeScanner* Win32VolumeScanner_Create(void);

#endif
