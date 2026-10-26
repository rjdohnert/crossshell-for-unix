#include "zstd.h"
#include "zstddeclib.h"

/* =========================================================================
 * MODULE: ZstdEngine (Enterprise Pipeline, Benchmark & Listing)
 * ========================================================================= */

static bool ZstdEngine_compress_impl(ZstdEngine *self, FILE *in, FILE *out, uint64_t in_size) {
    uint32_t magic = ZSTD_MAGIC;
    if (fwrite(&magic, 1, 4, out) != 4) return false;

    /* Frame Header Descriptor (RFC 8878 Section 3.1.1.1.1) */
    uint8_t fhd = 0;
    if (self->cfg->checksum) fhd |= (1 << 2); /* Content_Checksum_Flag */
    if (self->cfg->content_size && in_size > 0) {
        if (in_size > 0xFFFFFFFFULL) {
            fhd |= (3 << 6); /* 8-byte frame content size */
        } else {
            fhd |= (2 << 6); /* 4-byte frame content size */
        }
    }
    fputc(fhd, out);
    fputc(0x48, out); /* Window descriptor (512 KB: Exponent=9, Mantissa=0) */

    if (self->cfg->content_size && in_size > 0) {
        if (in_size > 0xFFFFFFFFULL) {
            uint64_t sz64 = in_size;
            fwrite(&sz64, 1, 8, out);
        } else {
            uint32_t sz32 = (uint32_t)in_size;
            fwrite(&sz32, 1, 4, out);
        }
    }

    void *hasher = NULL;
    if (self->cfg->checksum) {
        hasher = ZSTD_checksum_create();
    }
    int nbWorkers = self->cfg->threads;
    if (nbWorkers < 1) nbWorkers = 1;

    int max_chain = 4;
    bool fast_mode = (self->cfg->fast_level > 0);
    if (fast_mode) {
        max_chain = 1;
    } else {
        if (self->cfg->level <= 1) max_chain = 2;
        else if (self->cfg->level <= 3) max_chain = 8;
        else if (self->cfg->level <= 9) max_chain = 32;
        else if (self->cfg->level <= 19) max_chain = 128;
        else max_chain = self->cfg->ultra ? 512 : 128;
    }

    uint8_t **in_bufs = (uint8_t **)malloc(sizeof(uint8_t *) * nbWorkers);
    uint8_t **out_bufs = (uint8_t **)malloc(sizeof(uint8_t *) * nbWorkers);
    size_t *chunk_comp_sz = (size_t *)malloc(sizeof(size_t) * nbWorkers);
    size_t *chunk_src_sz = (size_t *)malloc(sizeof(size_t) * nbWorkers);
    bool *chunk_is_rle = (bool *)malloc(sizeof(bool) * nbWorkers);

    for (int w = 0; w < nbWorkers; w++) {
        in_bufs[w] = (uint8_t *)malloc(CHUNK_BLOCK_SIZE);
        out_bufs[w] = (uint8_t *)malloc(CHUNK_BLOCK_SIZE + 1024);
        chunk_comp_sz[w] = 0;
        chunk_src_sz[w] = 0;
        chunk_is_rle[w] = false;
    }

    ZstdThreadPool *pool = NULL;
    ZstdWorkerContext *single_ctx = NULL;
    if (nbWorkers > 1) {
        pool = ZstdThreadPool_Create(nbWorkers, max_chain, fast_mode);
    } else {
        single_ctx = (ZstdWorkerContext *)calloc(1, sizeof(ZstdWorkerContext));
    }

    uint64_t total_in = 0;
    uint64_t total_out = 0;
    double t_start = get_time_sec();
    bool is_tty = _isatty(_fileno(stderr));
    bool stream_eof = false;
    bool last_block_written = false;

    while (!stream_eof) {
        int active_chunks = 0;
        for (int w = 0; w < nbWorkers; w++) {
            size_t n = fread(in_bufs[w], 1, CHUNK_BLOCK_SIZE, in);
            if (n == 0) {
                stream_eof = true;
                break;
            }

            total_in += n;
            chunk_src_sz[w] = n;
            if (n < CHUNK_BLOCK_SIZE || feof(in) || (in_size > 0 && total_in >= in_size)) {
                stream_eof = true;
            }

            if (hasher) {
                ZSTD_checksum_update(hasher, in_bufs[w], n);
            }

            if (pool) {
                pool->workers[w].src = in_bufs[w];
                pool->workers[w].src_sz = n;
                pool->workers[w].dst = out_bufs[w];
                pool->workers[w].dst_cap = CHUNK_BLOCK_SIZE + 1024;
                pool->workers[w].comp_sz = 0;
                pool->workers[w].is_rle = false;
            } else {
                if (n > 0 && is_all_same_byte(in_bufs[w], n)) {
                    chunk_is_rle[w] = true;
                    chunk_comp_sz[w] = 1;
                } else {
                    chunk_is_rle[w] = false;
                    chunk_comp_sz[w] = ZstdCodec_compress_block_core(single_ctx, in_bufs[w], n,
                                                                     out_bufs[w], CHUNK_BLOCK_SIZE + 1024,
                                                                     max_chain, fast_mode);
                }
            }
            active_chunks++;
            if (stream_eof) break;
        }

        if (active_chunks == 0) break;

        if (pool) {
            pool->remaining_workers = active_chunks;
            MemoryBarrier();
            for (int w = 0; w < active_chunks; w++) {
                SetEvent(pool->workers[w].hStartEvent);
            }
            WaitForSingleObject(pool->hAllDoneEvent, INFINITE);
        }

        /* Write Chunks to Output */
        for (int i = 0; i < active_chunks; i++) {
            bool is_last = stream_eof && (i == active_chunks - 1);
            size_t src_size = pool ? pool->workers[i].src_sz : chunk_src_sz[i];
            bool is_rle = pool ? pool->workers[i].is_rle : chunk_is_rle[i];

            uint32_t bheader = 0;
            if (is_last) {
                bheader |= 1U;
                last_block_written = true;
            }

            if (is_rle) {
                /* RFC 8878 RLE Block (Block_Type = 1) */
                bheader |= (1U << 1);
                bheader |= ((uint32_t)src_size << 3);
                uint8_t hdr[3] = { (uint8_t)(bheader & 0xFF), (uint8_t)((bheader >> 8) & 0xFF), (uint8_t)((bheader >> 16) & 0xFF) };
                fwrite(hdr, 1, 3, out);
                fputc(in_bufs[i][0], out);
                total_out += 4;
            } else {
                /* RFC 8878 Raw Block (Block_Type = 0) */
                bheader |= (0U << 1);
                bheader |= ((uint32_t)src_size << 3);
                uint8_t hdr[3] = { (uint8_t)(bheader & 0xFF), (uint8_t)((bheader >> 8) & 0xFF), (uint8_t)((bheader >> 16) & 0xFF) };
                fwrite(hdr, 1, 3, out);
                fwrite(in_bufs[i], 1, src_size, out);
                total_out += 3 + src_size;
            }
        }

        if (is_tty && !self->cfg->quiet && !self->cfg->no_progress) {
            double elapsed = get_time_sec() - t_start;
            double speed = (elapsed > 0) ? ((double)total_in / (1024.0 * 1024.0)) / elapsed : 0.0;
            double ratio = (total_in > 0) ? ((double)total_out / (double)total_in) * 100.0 : 0.0;
            fprintf(stderr, "\rCompressed: %llu MB | Speed: %.1f MB/s | Ratio: %.1f%%  ",
                    (unsigned long long)(total_in / (1024 * 1024)), speed, ratio);
            fflush(stderr);
        }
    }

    if (!last_block_written) {
        uint8_t empty_hdr[3] = { 1, 0, 0 };
        fwrite(empty_hdr, 1, 3, out);
    }

    if (hasher) {
        uint32_t chk = ZSTD_checksum_digest(hasher);
        fwrite(&chk, 1, 4, out);
        ZSTD_checksum_free(hasher);
    }

    if (is_tty && !self->cfg->quiet && !self->cfg->no_progress) {
        fprintf(stderr, "\n");
    }

    if (pool) {
        ZstdThreadPool_Destroy(pool);
    }

    for (int w = 0; w < nbWorkers; w++) {
        free(in_bufs[w]);
        free(out_bufs[w]);
    }
    free(in_bufs);
    free(out_bufs);
    free(chunk_comp_sz);
    free(chunk_src_sz);
    free(chunk_is_rle);
    if (single_ctx) free(single_ctx);
    return true;
}

static bool ZstdEngine_decompress_impl(ZstdEngine *self, FILE *in, FILE *out) {
    (void)self;
    ZSTD_DStream *dstream = ZSTD_createDStream();
    if (!dstream) {
        fprintf(stderr, "zstd: cannot allocate decompression stream\n");
        return false;
    }

    size_t in_buf_size = ZSTD_DStreamInSize();
    size_t out_buf_size = ZSTD_DStreamOutSize();
    if (in_buf_size < 128 * 1024) in_buf_size = 128 * 1024;
    if (out_buf_size < 128 * 1024) out_buf_size = 128 * 1024;

    uint8_t *in_buf = (uint8_t *)malloc(in_buf_size);
    uint8_t *out_buf = (uint8_t *)malloc(out_buf_size);
    if (!in_buf || !out_buf) {
        if (in_buf) free(in_buf);
        if (out_buf) free(out_buf);
        ZSTD_freeDStream(dstream);
        return false;
    }

    size_t init_res = ZSTD_initDStream(dstream);
    if (ZSTD_isError(init_res)) {
        fprintf(stderr, "zstd: %s\n", ZSTD_getErrorName(init_res));
        free(in_buf); free(out_buf);
        ZSTD_freeDStream(dstream);
        return false;
    }

    ZSTD_inBuffer input = { in_buf, 0, 0 };
    bool ok = true;
    size_t last_ret = 0;

    while (1) {
        if (input.pos >= input.size) {
            input.size = fread(in_buf, 1, in_buf_size, in);
            input.pos = 0;
            if (input.size == 0) break;
        }

        ZSTD_outBuffer output = { out_buf, out_buf_size, 0 };
        size_t const ret = ZSTD_decompressStream(dstream, &output, &input);
        if (ZSTD_isError(ret)) {
            fprintf(stderr, "zstd: %s\n", ZSTD_getErrorName(ret));
            ok = false;
            break;
        }
        last_ret = ret;

        if (out && output.pos > 0) {
            if (fwrite(out_buf, 1, output.pos, out) != output.pos) {
                fprintf(stderr, "zstd: write error\n");
                ok = false;
                break;
            }
        }
    }

    if (ok && last_ret != 0) {
        fprintf(stderr, "zstd: unexpected end of compressed file\n");
        ok = false;
    }

    free(in_buf);
    free(out_buf);
    ZSTD_freeDStream(dstream);
    return ok;
}

static bool ZstdEngine_list_impl(ZstdEngine *self, const char *path) {
    (void)self;
    FILE *in = fopen(path, "rb");
    if (!in) {
        fprintf(stderr, "zstd: cannot open %s\n", path);
        return false;
    }

    _fseeki64(in, 0, SEEK_END);
    uint64_t comp_sz = _ftelli64(in);
    _fseeki64(in, 0, SEEK_SET);

    size_t probe_sz = (size_t)(comp_sz < 1024 * 1024 ? comp_sz : 1024 * 1024);
    uint8_t *probe = (uint8_t *)malloc(probe_sz);
    if (!probe) { fclose(in); return false; }
    if (fread(probe, 1, probe_sz, in) != probe_sz) {
        free(probe); fclose(in); return false;
    }
    fclose(in);

    unsigned long long orig_sz = ZSTD_findDecompressedSize(probe, probe_sz);
    bool has_chk = true;
    if (probe_sz >= 5 && (probe[4] & (1 << 2)) == 0) {
        has_chk = false;
    }
    free(probe);

    if (orig_sz == ZSTD_CONTENTSIZE_ERROR) {
        fprintf(stderr, "zstd: %s: not in zstd format\n", path);
        return false;
    }

    double ratio = 0.0;
    if (orig_sz != ZSTD_CONTENTSIZE_UNKNOWN && orig_sz > 0) {
        ratio = ((double)comp_sz / (double)orig_sz) * 100.0;
    } else {
        orig_sz = 0;
    }

    printf("Frames  Compressed  Uncompressed  Ratio  Check  Filename\n");
    printf("     1  %10llu  %12llu  %5.1f%%  %s  %s\n",
           (unsigned long long)comp_sz, (unsigned long long)orig_sz, ratio,
           has_chk ? "XXH64" : "None ", path);
    return true;
}


static bool ZstdEngine_benchmark_impl(ZstdEngine *self, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "zstd: cannot open %s\n", path);
        return false;
    }
    _fseeki64(f, 0, SEEK_END);
    size_t sz = (size_t)_ftelli64(f);
    _fseeki64(f, 0, SEEK_SET);

    uint8_t *src = (uint8_t *)malloc(sz);
    if (!src || (sz > 0 && fread(src, 1, sz, f) != sz)) {
        fclose(f); free(src);
        return false;
    }
    fclose(f);

    size_t comp_cap = sz + (sz / 4) + 65536;
    uint8_t *comp = (uint8_t *)malloc(comp_cap);
    uint8_t *decomp = (uint8_t *)malloc(sz + 65536);
    ZstdWorkerContext *ctx = (ZstdWorkerContext *)calloc(1, sizeof(ZstdWorkerContext));

    if (!comp || !decomp || !ctx) {
        free(src); free(comp); free(decomp); free(ctx);
        return false;
    }

    int max_chain = 4;
    bool fast_mode = (self->cfg->fast_level > 0);
    if (fast_mode) max_chain = 1;
    else if (self->cfg->level <= 1) max_chain = 2;
    else if (self->cfg->level <= 3) max_chain = 8;
    else if (self->cfg->level <= 9) max_chain = 32;
    else if (self->cfg->level <= 19) max_chain = 128;
    else max_chain = self->cfg->ultra ? 512 : 128;

    printf("Benchmarking %s (%llu bytes) at level %d (%d thread%s):\n",
           path, (unsigned long long)sz, self->cfg->level, self->cfg->threads,
           self->cfg->threads > 1 ? "s" : "");

    double t0 = get_time_sec();
    size_t c_size = 0;
    if (sz > 0 && is_all_same_byte(src, sz)) {
        c_size = 1;
        comp[0] = src[0];
    } else {
        c_size = ZstdCodec_compress_block_core(ctx, src, sz, comp, comp_cap, max_chain, fast_mode);
    }
    double c_time = get_time_sec() - t0;
    double c_speed = (c_time > 0) ? ((double)sz / (1024.0 * 1024.0)) / c_time : 0.0;
    double ratio = (sz > 0 && c_size > 0) ? ((double)c_size / (double)sz) * 100.0 : 100.0;

    double t1 = get_time_sec();
    size_t d_size = 0;
    if (sz > 0 && is_all_same_byte(src, sz)) {
        d_size = sz;
        memset(decomp, comp[0], sz);
    } else if (c_size > 0) {
        d_size = ZstdCodec_decompress_block_core(comp, c_size, decomp, sz + 65536);
    } else {
        d_size = sz;
        memcpy(decomp, src, sz);
    }
    double d_time = get_time_sec() - t1;
    double d_speed = (d_time > 0) ? ((double)sz / (1024.0 * 1024.0)) / d_time : 0.0;

    bool match = (d_size == sz && (sz == 0 || memcmp(src, decomp, sz) == 0));

    printf("Level %2d : Comp: %7.1f MB/s, Decomp: %7.1f MB/s, Ratio: %5.1f%% (%s)\n",
           self->cfg->level, c_speed, d_speed, ratio, match ? "Verified OK" : "MISMATCH");

    free(src); free(comp); free(decomp); free(ctx);
    return match;
}

static int ZstdEngine_run_impl(ZstdEngine *self) {
    if (self->cfg->mode == MODE_BENCHMARK) {
        if (self->cfg->input_file_count == 0) {
            fprintf(stderr, "zstd: benchmark requires file argument\n");
            return 1;
        }
        for (int i = 0; i < self->cfg->input_file_count; i++) {
            self->benchmark(self, self->cfg->input_files[i]);
        }
        return 0;
    }

    if (self->cfg->mode == MODE_LIST) {
        if (self->cfg->input_file_count == 0) {
            fprintf(stderr, "zstd: list requires file argument\n");
            return 1;
        }
        for (int i = 0; i < self->cfg->input_file_count; i++) {
            self->list(self, self->cfg->input_files[i]);
        }
        return 0;
    }

    int total_files = self->cfg->input_file_count ? self->cfg->input_file_count : 1;

    for (int idx = 0; idx < total_files; idx++) {
        const char *src_path = self->cfg->input_file_count ? self->cfg->input_files[idx] : "-";
        FILE *in = stdin;
        FILE *out = stdout;
        uint64_t in_size = 0;
        char dst_path[MAX_PATH_EXTENDED] = "";

        if (strcmp(src_path, "-") != 0) {
            in = fopen(src_path, "rb");
            if (!in) {
                fprintf(stderr, "zstd: cannot open %s\n", src_path);
                return 1;
            }
            _fseeki64(in, 0, SEEK_END);
            in_size = _ftelli64(in);
            _fseeki64(in, 0, SEEK_SET);

            if (self->cfg->output_file[0] != '\0') {
                strncpy(dst_path, self->cfg->output_file, MAX_PATH_EXTENDED - 1);
            } else if (!self->cfg->to_stdout && self->cfg->mode != MODE_TEST) {
                if (self->cfg->mode == MODE_COMPRESS) {
                    snprintf(dst_path, sizeof(dst_path), "%s.zst", src_path);
                } else if (self->cfg->mode == MODE_DECOMPRESS) {
                    size_t slen = strlen(src_path);
                    if (slen > 4 && strcmp(&src_path[slen - 4], ".zst") == 0) {
                        strncpy(dst_path, src_path, slen - 4);
                        dst_path[slen - 4] = '\0';
                    } else {
                        snprintf(dst_path, sizeof(dst_path), "%s.out", src_path);
                    }
                }
            }
        }

        if (dst_path[0] != '\0') {
            if (!self->cfg->force) {
                FILE *test = fopen(dst_path, "rb");
                if (test) {
                    fclose(test);
                    fprintf(stderr, "zstd: %s already exists; use -f to overwrite\n", dst_path);
                    if (in != stdin) fclose(in);
                    return 1;
                }
            }
            out = _fsopen(dst_path, "wb", _SH_DENYWR);
            if (!out) {
                out = fopen(dst_path, "wb");
            }
            if (!out) {
                fprintf(stderr, "zstd: cannot create %s\n", dst_path);
                if (in != stdin) fclose(in);
                return 1;
            }
            EnterCriticalSection(&g_cleanup_lock);
            strncpy(g_active_output_file, dst_path, MAX_PATH_EXTENDED - 1);
            LeaveCriticalSection(&g_cleanup_lock);
        }

        char *in_buf = (char *)malloc(IO_BUFFER_SIZE);
        char *out_buf = (char *)malloc(IO_BUFFER_SIZE);
        if (in_buf && in != stdin) setvbuf(in, in_buf, _IOFBF, IO_BUFFER_SIZE);
        if (out_buf && out != stdout && out != NULL) setvbuf(out, out_buf, _IOFBF, IO_BUFFER_SIZE);

        bool ok = true;
        if (self->cfg->mode == MODE_COMPRESS) {
            ok = self->compress(self, in, out, in_size);
        } else if (self->cfg->mode == MODE_DECOMPRESS) {
            ok = self->decompress(self, in, out);
        } else if (self->cfg->mode == MODE_TEST) {
            ok = self->decompress(self, in, NULL);
            if (!self->cfg->quiet) {
                printf("%s : %s\n", src_path, ok ? "OK" : "CORRUPTED");
            }
        }

        if (in != stdin) fclose(in);
        if (out != stdout && out != NULL) fclose(out);
        free(in_buf);
        free(out_buf);

        EnterCriticalSection(&g_cleanup_lock);
        g_active_output_file[0] = '\0';
        LeaveCriticalSection(&g_cleanup_lock);

        if (!ok) {
            if (dst_path[0] != '\0') DeleteFileA(dst_path);
            return 1;
        }

        if (self->cfg->rm_source && strcmp(src_path, "-") != 0) {
            DeleteFileA(src_path);
        }
    }

    return 0;
}

ZstdEngine *ZstdEngine_New(ZstdConfig *cfg) {
    ZstdEngine *self = (ZstdEngine *)calloc(1, sizeof(ZstdEngine));
    self->cfg = cfg;
    self->run = ZstdEngine_run_impl;
    self->compress = ZstdEngine_compress_impl;
    self->decompress = ZstdEngine_decompress_impl;
    self->list = ZstdEngine_list_impl;
    self->benchmark = ZstdEngine_benchmark_impl;
    return self;
}
