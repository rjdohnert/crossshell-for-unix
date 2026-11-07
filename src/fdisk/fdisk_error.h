#ifndef FDISK_ERROR_H
#define FDISK_ERROR_H

#include "fdisk_types.h"

void FdiskErrorClear(FdiskError* error);
void FdiskErrorSet(FdiskError* error, DWORD code, const wchar_t* message);
void FdiskErrorSetWindows(FdiskError* error, const wchar_t* operation, DWORD code);

#endif
