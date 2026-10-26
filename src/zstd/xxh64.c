#include "zstd.h"

/* =========================================================================
 * MODULE: Xxh64 Streaming Checksum Engine (RFC 8878 Section 3.1.1.1.4)
 * ========================================================================= */

#define XXH64_PRIME1 11400714785074694791ULL
#define XXH64_PRIME2 14029467366897019727ULL
#define XXH64_PRIME3 1609587929392839161ULL
#define XXH64_PRIME4 9650029242287828579ULL
#define XXH64_PRIME5 287017744541269605ULL

static inline uint64_t xxh64_rotl(uint64_t x, int r) {
    return (x << r) | (x >> (64 - r));
}

static inline uint64_t xxh64_round(uint64_t acc, uint64_t input) {
    acc += input * XXH64_PRIME2;
    acc = xxh64_rotl(acc, 31);
    acc *= XXH64_PRIME1;
    return acc;
}

static inline uint64_t xxh64_merge_round(uint64_t acc, uint64_t val) {
    val = xxh64_round(0, val);
    acc ^= val;
    acc = acc * XXH64_PRIME1 + XXH64_PRIME4;
    return acc;
}

static void Xxh64_reset(Xxh64 *self) {
    self->v1 = XXH64_PRIME1 + XXH64_PRIME2;
    self->v2 = XXH64_PRIME2;
    self->v3 = 0;
    self->v4 = 0 - XXH64_PRIME1;
    self->total_len = 0;
    self->mem_size = 0;
}

static void Xxh64_update(Xxh64 *self, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    const uint8_t *bEnd = p + len;
    self->total_len += len;

    if (self->mem_size + len < 32) {
        memcpy(self->mem + self->mem_size, p, len);
        self->mem_size += len;
        return;
    }

    if (self->mem_size > 0) {
        size_t fill = 32 - self->mem_size;
        memcpy(self->mem + self->mem_size, p, fill);
        uint64_t k1, k2, k3, k4;
        memcpy(&k1, self->mem, 8);
        memcpy(&k2, self->mem + 8, 8);
        memcpy(&k3, self->mem + 16, 8);
        memcpy(&k4, self->mem + 24, 8);
        self->v1 = xxh64_round(self->v1, k1);
        self->v2 = xxh64_round(self->v2, k2);
        self->v3 = xxh64_round(self->v3, k3);
        self->v4 = xxh64_round(self->v4, k4);
        p += fill;
        self->mem_size = 0;
    }

    while (p <= bEnd - 32) {
        uint64_t k1, k2, k3, k4;
        memcpy(&k1, p, 8);
        memcpy(&k2, p + 8, 8);
        memcpy(&k3, p + 16, 8);
        memcpy(&k4, p + 24, 8);
        self->v1 = xxh64_round(self->v1, k1);
        self->v2 = xxh64_round(self->v2, k2);
        self->v3 = xxh64_round(self->v3, k3);
        self->v4 = xxh64_round(self->v4, k4);
        p += 32;
    }

    if (p < bEnd) {
        self->mem_size = (size_t)(bEnd - p);
        memcpy(self->mem, p, self->mem_size);
    }
}

static uint64_t Xxh64_digest(Xxh64 *self) {
    uint64_t h64;
    if (self->total_len >= 32) {
        h64 = xxh64_rotl(self->v1, 1) + xxh64_rotl(self->v2, 7) +
              xxh64_rotl(self->v3, 12) + xxh64_rotl(self->v4, 18);
        h64 = xxh64_merge_round(h64, self->v1);
        h64 = xxh64_merge_round(h64, self->v2);
        h64 = xxh64_merge_round(h64, self->v3);
        h64 = xxh64_merge_round(h64, self->v4);
    } else {
        h64 = self->v3 + XXH64_PRIME5;
    }
    h64 += self->total_len;

    const uint8_t *p = self->mem;
    const uint8_t *bEnd = self->mem + self->mem_size;
    while (p + 8 <= bEnd) {
        uint64_t k1;
        memcpy(&k1, p, 8);
        k1 = xxh64_round(0, k1);
        h64 ^= k1;
        h64 = xxh64_rotl(h64, 27) * XXH64_PRIME1 + XXH64_PRIME4;
        p += 8;
    }
    if (p + 4 <= bEnd) {
        uint32_t k1;
        memcpy(&k1, p, 4);
        h64 ^= (uint64_t)k1 * XXH64_PRIME1;
        h64 = xxh64_rotl(h64, 23) * XXH64_PRIME2 + XXH64_PRIME3;
        p += 4;
    }
    while (p < bEnd) {
        h64 ^= ((uint64_t)(*p)) * XXH64_PRIME5;
        h64 = xxh64_rotl(h64, 11) * XXH64_PRIME1;
        p++;
    }

    h64 ^= h64 >> 33;
    h64 *= XXH64_PRIME2;
    h64 ^= h64 >> 29;
    h64 *= XXH64_PRIME3;
    h64 ^= h64 >> 32;
    return h64;
}

static uint32_t Xxh64_digest32(Xxh64 *self) {
    return (uint32_t)(Xxh64_digest(self) & 0xFFFFFFFFU);
}

Xxh64 Xxh64_Create(void) {
    Xxh64 x;
    x.reset = Xxh64_reset;
    x.update = Xxh64_update;
    x.digest = Xxh64_digest;
    x.digest32 = Xxh64_digest32;
    x.reset(&x);
    return x;
}
