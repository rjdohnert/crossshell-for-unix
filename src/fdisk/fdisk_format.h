#ifndef FDISK_FORMAT_H
#define FDISK_FORMAT_H

#include "fdisk_types.h"

bool FdiskPrintDisk(const FdiskDisk* disk, bool sector_units, FdiskError* error);
void FdiskPrintUsage(void);
void FdiskPrintVersion(void);

#endif
