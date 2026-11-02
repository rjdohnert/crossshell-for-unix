#ifndef CPIO_PATH_H
#define CPIO_PATH_H

#include "cpio.h"

// UTF-8 <-> UTF-16 conversion and Win32 extended-path handling.
wchar_t* Utf8ToWide(const char* utf8);
char*    WideToUtf8(const wchar_t* wide);
wchar_t* PrepareWin32Path(const wchar_t* wpath);
void     NormalizeSlashes(char* path);

// UTF-8-aware filesystem wrappers.
FILE*  FileOpenUtf8(const char* path, const char* mode);
int    Stat64Utf8(const char* path, struct _stat64* st);
int    MakeDirUtf8(const char* path);
int    UnlinkUtf8(const char* path);
DWORD  GetFileAttrUtf8(const char* path);
BOOL   SetFileAttrUtf8(const char* path, DWORD attr);
int    Utime64Utf8(const char* path, struct __utimbuf64* ut);
bool   CreateHardLinkUtf8(const char* targetPath, const char* existingPath);
bool   CreateSymlinkUtf8(const char* targetPath, const char* symlinkTarget, bool isDir);
bool   CopyFileFallback(const char* src, const char* dst);
bool   IsSamePhysicalFile(const char* path1, const char* path2);
void   MakeDirectoryTree(const char* path);

#endif // CPIO_PATH_H
