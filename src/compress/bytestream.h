#ifndef BYTESTREAM_H
#define BYTESTREAM_H

#include "common.h"

typedef struct IByteStream IByteStream;

typedef struct {
    size_t   (*read)(IByteStream *this, void *buf, size_t size);
    size_t   (*write)(IByteStream *this, const void *buf, size_t size);
    bool     (*flush)(IByteStream *this);
    uint64_t (*get_bytes_transferred)(const IByteStream *this);
    bool     (*has_error)(const IByteStream *this);
    void     (*destroy)(IByteStream *this);
} IByteStreamVtbl;

struct IByteStream {
    const IByteStreamVtbl *vptr;
};

IByteStream *FileStream_create(const wchar_t *path, DWORD access, DWORD creation);
IByteStream *StdioStream_create(DWORD stdHandleType);

#endif
