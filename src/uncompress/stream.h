#ifndef STREAM_H
#define STREAM_H

#include "common.h"

typedef struct IStream IStream;

typedef struct IStreamVtbl {
    int64_t (*Read)(IStream* self, void* buffer, size_t size);
    int64_t (*Write)(IStream* self, const void* buffer, size_t size);
    void    (*Close)(IStream* self);
    void    (*Destroy)(IStream* self);
} IStreamVtbl;

struct IStream {
    const IStreamVtbl* vptr;
};

typedef struct FileStream {
    IStream base;
    HANDLE  handle;
    bool    ownsHandle;
    bool    isEof;
} FileStream;

FileStream* FileStream_Create(HANDLE handle, bool ownsHandle);
FileStream* StdStream_Create(DWORD stdHandleType);

#endif
