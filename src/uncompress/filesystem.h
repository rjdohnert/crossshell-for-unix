#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "common.h"

typedef struct FileSystemManager {
    bool force;
    bool keep;
    bool quiet;
} FileSystemManager;

FileSystemManager* FileSystemManager_Create(bool force, bool keep, bool quiet);
void FileSystemManager_Destroy(FileSystemManager* self);
bool FileSystemManager_HasZSuffix(const wchar_t* path);
bool FileSystemManager_PromptOverwrite(const wchar_t* targetPath);
void FileSystemManager_PreserveTimestamps(HANDLE hSrc, HANDLE hDst);

// Temp file tracking for Ctrl-C cleanup
extern wchar_t g_ActiveTempPath[PATH_BUFFER_CAPACITY];
extern HANDLE  g_ActiveTempHandle;

#endif
