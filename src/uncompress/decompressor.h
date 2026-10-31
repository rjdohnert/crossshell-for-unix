#ifndef DECOMPRESSOR_H
#define DECOMPRESSOR_H

#include "common.h"
#include "stream.h"

typedef enum DecompressResult {
    DECOMP_OK = 0,
    DECOMP_NOT_COMPRESSED,
    DECOMP_CORRUPT,
    DECOMP_IO_ERROR,
    DECOMP_MEM_ERROR
} DecompressResult;

typedef struct LzwDecompressor {
    uint64_t bytesIn;
    uint64_t bytesOut;
} LzwDecompressor;

LzwDecompressor* LzwDecompressor_Create(void);
void LzwDecompressor_Destroy(LzwDecompressor* self);
DecompressResult LzwDecompressor_Process(LzwDecompressor* self, IStream* inStream, IStream* outStream);

#endif
