#include "zstd.h"

/* =========================================================================
 * MODULE: ZstdCodec (High-Performance Match Finder & Block Codec)
 * ========================================================================= */

static inline size_t common_prefix_len(const uint8_t *s1, const uint8_t *s2, size_t max_len) {
    size_t len = 0;
    while (len + 8 <= max_len) {
        uint64_t a, b;
        memcpy(&a, s1 + len, 8);
        memcpy(&b, s2 + len, 8);
        if (a != b) {
            uint64_t diff = a ^ b;
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
            unsigned long idx;
            _BitScanForward64(&idx, diff);
            return len + (idx >> 3);
#elif defined(_MSC_VER) && defined(_M_IX86)
            unsigned long idx;
            if ((uint32_t)diff != 0) {
                _BitScanForward(&idx, (uint32_t)diff);
                return len + (idx >> 3);
            } else {
                _BitScanForward(&idx, (uint32_t)(diff >> 32));
                return len + 4 + (idx >> 3);
            }
#elif defined(__GNUC__) || defined(__clang__)
            return len + (__builtin_ctzll(diff) >> 3);
#else
            size_t sub = 0;
            while (len + sub < max_len && s1[len + sub] == s2[len + sub]) sub++;
            return len + sub;
#endif
        }
        len += 8;
    }
    while (len < max_len && s1[len] == s2[len]) {
        len++;
    }
    return len;
}

bool is_all_same_byte(const uint8_t *src, size_t sz) {
    if (sz <= 1) return true;
    uint8_t b = src[0];
    size_t i = 1;
    uint64_t b8 = ((uint64_t)b) * 0x0101010101010101ULL;
    while (i + 8 <= sz) {
        uint64_t v;
        memcpy(&v, src + i, 8);
        if (v != b8) return false;
        i += 8;
    }
    while (i < sz) {
        if (src[i] != b) return false;
        i++;
    }
    return true;
}

static inline uint32_t hash4(const uint8_t *p, int hash_bits) {
    uint32_t v;
    memcpy(&v, p, 4);
    return (v * 2654435761U) >> (32 - hash_bits);
}

size_t ZstdCodec_compress_block_core(ZstdWorkerContext *ctx,
                                     const uint8_t *src, size_t src_sz,
                                     uint8_t *dst, size_t dst_cap,
                                     int max_chain, bool fast_mode) {
    if (src_sz == 0 || dst_cap < src_sz + 64) return 0;

    int hash_bits = fast_mode ? HASH_BITS_FAST : HASH_BITS_DEFAULT;
    int hash_size = 1 << hash_bits;

    memset(ctx->head, 0xFF, (size_t)hash_size * sizeof(int32_t));

    size_t ip = 0;
    size_t op = 0;
    size_t anchor = 0;
    size_t step = 1;

    while (ip + MIN_MATCH <= src_sz) {
        if (op + (ip - anchor) + 64 >= dst_cap) {
            return 0; /* Incompressible */
        }

        uint32_t h = hash4(&src[ip], hash_bits);
        int32_t match_pos = ctx->head[h];
        ctx->prev[ip % WINDOW_SIZE] = match_pos;
        ctx->head[h] = (int32_t)ip;

        size_t best_len = 0;
        size_t best_offset = 0;
        int chain = max_chain;

        while (match_pos != -1 && chain-- > 0) {
            if ((ip - match_pos) >= WINDOW_SIZE) break;

            uint32_t m_val, cur_val;
            memcpy(&m_val, &src[match_pos], 4);
            memcpy(&cur_val, &src[ip], 4);

            if (m_val == cur_val) {
                size_t max_avail = src_sz - ip;
                if (max_avail > 65535) max_avail = 65535;
                size_t len = common_prefix_len(&src[match_pos], &src[ip], max_avail);
                if (len > best_len) {
                    best_len = len;
                    best_offset = ip - match_pos;
                    if (best_len >= 128 || fast_mode) break;
                }
            }
            if (fast_mode) break;
            match_pos = ctx->prev[match_pos % WINDOW_SIZE];
        }

        if (best_len >= MIN_MATCH) {
            step = 1;
            size_t lit_len = ip - anchor;
            if (op + lit_len + 32 >= dst_cap) return 0;

            uint8_t *token = &dst[op++];
            size_t token_lit = (lit_len >= 15) ? 15 : lit_len;
            size_t token_mat = ((best_len - MIN_MATCH) >= 15) ? 15 : (best_len - MIN_MATCH);
            *token = (uint8_t)((token_lit << 4) | token_mat);

            if (lit_len >= 15) {
                size_t rem = lit_len - 15;
                while (rem >= 255) { dst[op++] = 255; rem -= 255; }
                dst[op++] = (uint8_t)rem;
            }
            if (lit_len > 0) {
                memcpy(&dst[op], &src[anchor], lit_len);
                op += lit_len;
            }

            dst[op++] = (uint8_t)(best_offset & 0xFF);
            dst[op++] = (uint8_t)((best_offset >> 8) & 0xFF);

            if ((best_len - MIN_MATCH) >= 15) {
                size_t rem = (best_len - MIN_MATCH) - 15;
                while (rem >= 255) { dst[op++] = 255; rem -= 255; }
                dst[op++] = (uint8_t)rem;
            }

            ip += best_len;
            anchor = ip;

            if (ip + 4 <= src_sz) {
                size_t start_k = (ip >= 2) ? (ip - 2) : 0;
                for (size_t k = start_k; k < ip && k + 4 <= src_sz; k++) {
                    uint32_t kh = hash4(&src[k], hash_bits);
                    ctx->prev[k % WINDOW_SIZE] = ctx->head[kh];
                    ctx->head[kh] = (int32_t)k;
                }
            }
        } else {
            if (fast_mode && (ip - anchor) > 32) {
                ip += (step++);
                if (step > 4) step = 4;
            } else {
                ip++;
            }
        }
    }

    size_t last_lit = src_sz - anchor;
    if (op + last_lit + 32 >= dst_cap) return 0;

    uint8_t *token = &dst[op++];
    size_t token_lit = (last_lit >= 15) ? 15 : last_lit;
    *token = (uint8_t)(token_lit << 4);

    if (last_lit >= 15) {
        size_t rem = last_lit - 15;
        while (rem >= 255) { dst[op++] = 255; rem -= 255; }
        dst[op++] = (uint8_t)rem;
    }
    if (last_lit > 0) {
        memcpy(&dst[op], &src[anchor], last_lit);
        op += last_lit;
    }

    if (op >= src_sz) return 0;
    return op;
}

size_t ZstdCodec_decompress_block_core(const uint8_t *src, size_t src_sz,
                                       uint8_t *dst, size_t dst_cap) {
    size_t ip = 0;
    size_t op = 0;

    while (ip < src_sz && op < dst_cap) {
        uint8_t token = src[ip++];
        size_t lit_len = token >> 4;
        size_t mat_len = (token & 0x0F) + MIN_MATCH;

        if (lit_len == 15) {
            while (ip < src_sz && src[ip] == 255) { lit_len += 255; ip++; }
            if (ip < src_sz) lit_len += src[ip++];
        }

        if (lit_len > 0) {
            if (op + lit_len > dst_cap || ip + lit_len > src_sz) return 0;
            memcpy(&dst[op], &src[ip], lit_len);
            op += lit_len;
            ip += lit_len;
        }

        if (ip >= src_sz) break;

        if (ip + 2 > src_sz) return 0;
        uint16_t offset = (uint16_t)src[ip] | ((uint16_t)src[ip + 1] << 8);
        ip += 2;
        if (offset == 0 || offset > op) return 0;

        if ((token & 0x0F) == 15) {
            while (ip < src_sz && src[ip] == 255) { mat_len += 255; ip++; }
            if (ip < src_sz) mat_len += src[ip++];
        }

        if (op + mat_len > dst_cap) return 0;
        size_t match_src = op - offset;

        if (offset >= 8) {
            while (mat_len >= 8 && op + 8 <= dst_cap) {
                memcpy(&dst[op], &dst[match_src], 8);
                op += 8;
                match_src += 8;
                mat_len -= 8;
            }
        }
        while (mat_len > 0 && op < dst_cap) {
            dst[op++] = dst[match_src++];
            mat_len--;
        }
    }

    return op;
}
