#ifndef METADATA_H
#define METADATA_H

#include "common.h"

typedef struct {
    FILETIME ftCreation;
    FILETIME ftLastAccess;
    FILETIME ftLastWrite;
    DWORD dwAttributes;
    bool valid;
} FileMetadata;

bool FileMetadata_capture(FileMetadata *this, const wchar_t *src_path);
bool FileMetadata_apply(const FileMetadata *this, const wchar_t *dst_path);

#endif
