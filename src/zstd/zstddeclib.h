#ifndef ZSTDDECLIB_H
#define ZSTDDECLIB_H

#include <stddef.h>

#define ZSTD_CONTENTSIZE_UNKNOWN (0ULL - 1)
#define ZSTD_CONTENTSIZE_ERROR   (0ULL - 2)

typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef struct ZSTD_DCtx_s ZSTD_DStream;

typedef struct ZSTD_inBuffer_s {
  const void* src;    /**< start of input buffer */
  size_t size;        /**< size of input buffer */
  size_t pos;         /**< position where reading stopped. Will be updated. Necessarily 0 <= pos <= size */
} ZSTD_inBuffer;

typedef struct ZSTD_outBuffer_s {
  void*  dst;         /**< start of output buffer */
  size_t size;        /**< size of output buffer */
  size_t pos;         /**< position where writing stopped. Will be updated. Necessarily 0 <= pos <= size */
} ZSTD_outBuffer;

size_t ZSTD_decompress(void* dst, size_t dstCapacity, const void* src, size_t compressedSize);
unsigned ZSTD_isError(size_t code);
const char* ZSTD_getErrorName(size_t code);
unsigned long long ZSTD_getFrameContentSize(const void *src, size_t srcSize);
unsigned long long ZSTD_findDecompressedSize(const void *src, size_t srcSize);

ZSTD_DStream* ZSTD_createDStream(void);
size_t ZSTD_freeDStream(ZSTD_DStream* zds);
size_t ZSTD_initDStream(ZSTD_DStream* zds);
size_t ZSTD_decompressStream(ZSTD_DStream* zds, ZSTD_outBuffer* output, ZSTD_inBuffer* input);
size_t ZSTD_DStreamInSize(void);
size_t ZSTD_DStreamOutSize(void);

void* ZSTD_checksum_create(void);
void ZSTD_checksum_update(void* ctx, const void* data, size_t len);
unsigned int ZSTD_checksum_digest(void* ctx);
void ZSTD_checksum_free(void* ctx);

#endif /* ZSTDDECLIB_H */
