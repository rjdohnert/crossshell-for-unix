#ifndef CODEC_H
#define CODEC_H

#include "common.h"
#include "bytestream.h"

typedef struct ICodec ICodec;

typedef struct {
    bool     (*execute)(ICodec *this, IByteStream *src, IByteStream *dst);
    uint64_t (*get_bytes_in)(const ICodec *this);
    uint64_t (*get_bytes_out)(const ICodec *this);
    void     (*destroy)(ICodec *this);
} ICodecVtbl;

struct ICodec {
    const ICodecVtbl *vptr;
};

ICodec *LzwCompressor_create(int max_bits);
ICodec *LzwDecompressor_create(void);

#endif
