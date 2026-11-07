#ifndef MKFS_ERROR_H
#define MKFS_ERROR_H

#include "mkfs_types.h"

void MkfsErrorClear(MkfsError* error);
void MkfsErrorSet(MkfsError* error, DWORD code, const wchar_t* message);
void MkfsErrorSetWindows(MkfsError* error, const wchar_t* operation, DWORD code);

#endif
