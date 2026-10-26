#ifndef ZSTD_H
#define ZSTD_H

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <io.h>
#include <fcntl.h>
#include <share.h>
#include <sys/stat.h>
#include <windows.h>

#if defined(_MSC_VER) || defined(__clang__)
#include <intrin.h>
#endif

/* =========================================================================
 * Constants & Memory Alignment
 * ========================================================================= */

#define ZSTD_CLI_VERSION        "v1.5.6-rfc8878"
#define ZSTD_MAGIC              0xFD2FB528U
#define CHUNK_BLOCK_SIZE        (128 * 1024)   /* 128 KB standard block */
#define WINDOW_SIZE             65536          /* 64 KB sliding window */
#define HASH_BITS_FAST          14             /* 16K entries for fast mode */
#define HASH_BITS_DEFAULT       16             /* 64K entries for standard */
#define HASH_SIZE_DEFAULT       (1 << HASH_BITS_DEFAULT)
#define MIN_MATCH               4
#define IO_BUFFER_SIZE          (1024 * 1024)  /* 1 MB sequential stream buffer */
#define MAX_PATH_EXTENDED       32768          /* Extended Win32 / UNC path limit */

#if defined(_MSC_VER)
#define CACHE_ALIGN __declspec(align(64))
#elif defined(__GNUC__) || defined(__clang__)
#define CACHE_ALIGN __attribute__((aligned(64)))
#else
#define CACHE_ALIGN
#endif

/* Global cleanup tracking for process interrupt */
extern CRITICAL_SECTION g_cleanup_lock;
extern char g_active_output_file[MAX_PATH_EXTENDED];

/* Platform timing and processor discovery */
double get_time_sec(void);
int detect_processor_count(void);
WORD detect_processor_group_count(void);
void assign_thread_to_group(HANDLE hThread, int worker_index, WORD group_count);

/* =========================================================================
 * MODULE: Xxh64 Streaming Checksum Engine (RFC 8878 Compliant)
 * ========================================================================= */

typedef struct Xxh64 {
    uint64_t v1, v2, v3, v4;
    uint64_t total_len;
    uint8_t  mem[32];
    size_t   mem_size;

    void (*reset)(struct Xxh64 *self);
    void (*update)(struct Xxh64 *self, const void *data, size_t len);
    uint64_t (*digest)(struct Xxh64 *self);
    uint32_t (*digest32)(struct Xxh64 *self);
} Xxh64;

Xxh64 Xxh64_Create(void);

/* =========================================================================
 * MODULE: ZstdCodec (High-Performance Match Finder & Block Codec)
 * ========================================================================= */

typedef struct CACHE_ALIGN ZstdWorkerContext {
    int32_t head[HASH_SIZE_DEFAULT];
    int32_t prev[WINDOW_SIZE];
} ZstdWorkerContext;

bool is_all_same_byte(const uint8_t *src, size_t sz);

size_t ZstdCodec_compress_block_core(ZstdWorkerContext *ctx,
                                     const uint8_t *src, size_t src_sz,
                                     uint8_t *dst, size_t dst_cap,
                                     int max_chain, bool fast_mode);

size_t ZstdCodec_decompress_block_core(const uint8_t *src, size_t src_sz,
                                       uint8_t *dst, size_t dst_cap);

/* =========================================================================
 * MODULE: ZstdThreadPool (Atomic Latch & Multi-Core Scaling)
 * ========================================================================= */

struct ZstdThreadPool;

typedef struct CACHE_ALIGN WorkerThreadData {
    HANDLE  hThread;
    HANDLE  hStartEvent;
    bool    terminate;

    const uint8_t *src;
    size_t         src_sz;
    uint8_t       *dst;
    size_t         dst_cap;
    size_t         comp_sz;
    bool           is_rle;

    int            max_chain;
    bool           fast_mode;
    struct ZstdThreadPool *pool;
    ZstdWorkerContext *ctx;
} WorkerThreadData;

typedef struct CACHE_ALIGN ZstdThreadPool {
    int               num_workers;
    WorkerThreadData *workers;
    HANDLE           *thread_handles;
    HANDLE            hAllDoneEvent;
    volatile LONG     remaining_workers;
} ZstdThreadPool;

ZstdThreadPool *ZstdThreadPool_Create(int threads, int max_chain, bool fast_mode);
void ZstdThreadPool_Destroy(ZstdThreadPool *pool);

/* =========================================================================
 * MODULE: ZstdConfig (Command Line Qualifiers & Parser)
 * ========================================================================= */

typedef enum {
    MODE_COMPRESS,
    MODE_DECOMPRESS,
    MODE_TEST,
    MODE_LIST,
    MODE_BENCHMARK
} OperationMode;

typedef struct ZstdConfig {
    OperationMode mode;
    int  level;
    bool ultra;
    int  fast_level;
    int  threads;
    bool checksum;
    bool content_size;
    bool force;
    bool keep;
    bool rm_source;
    bool to_stdout;
    bool quiet;
    int  verbose;
    bool no_progress;
    char output_file[MAX_PATH_EXTENDED];
    char **input_files;
    int  input_file_count;

    void (*print_help)(void);
    void (*print_version)(void);
    bool (*parse)(struct ZstdConfig *self, int argc, char *argv[]);
} ZstdConfig;

ZstdConfig *ZstdConfig_New(void);

/* =========================================================================
 * MODULE: ZstdEngine (Enterprise Pipeline, Benchmark & Listing)
 * ========================================================================= */

typedef struct ZstdEngine {
    ZstdConfig *cfg;

    int  (*run)(struct ZstdEngine *self);
    bool (*compress)(struct ZstdEngine *self, FILE *in, FILE *out, uint64_t in_size);
    bool (*decompress)(struct ZstdEngine *self, FILE *in, FILE *out);
    bool (*list)(struct ZstdEngine *self, const char *path);
    bool (*benchmark)(struct ZstdEngine *self, const char *path);
} ZstdEngine;

ZstdEngine *ZstdEngine_New(ZstdConfig *cfg);

#endif /* ZSTD_H */
