#include "codec.h"
#include "bitio.h"

typedef struct {
    ICodec base;
    int max_bits;
    bool block_mode;
    uint64_t bytes_in;
    uint64_t bytes_out;
    int32_t *codetab;
    uint32_t *htab;
} LzwCompressor;

static void LzwCompressor_clear_table(LzwCompressor *this) {
    memset(this->codetab, -1, HASH_SIZE * sizeof(int32_t));
}

static bool LzwCompressor_execute(ICodec *super, IByteStream *src, IByteStream *dst) {
    LzwCompressor *this = (LzwCompressor *)super;

    uint8_t header[3] = {
        MAGIC_BYTE_0,
        MAGIC_BYTE_1,
        (uint8_t)((this->max_bits & BITS_MASK) | (this->block_mode ? BLOCK_MASK : 0))
    };
    if (dst->vptr->write(dst, header, 3) != 3) return false;

    BitWriter *writer = BitWriter_create(dst);
    if (!writer) return false;

    int n_bits = INIT_BITS;
    uint32_t maxcode = (1U << n_bits) - 1;
    uint32_t free_ent = this->block_mode ? FIRST_FREE_CODE : 256;
    LzwCompressor_clear_table(this);

    uint64_t checkpoint = CHECK_GAP;
    double prev_ratio = 0.0;

    uint8_t in_buf[IO_BUFFER_SIZE];
    size_t bytes_read = src->vptr->read(src, in_buf, IO_BUFFER_SIZE);
    if (bytes_read == 0) {
        bool flush_ok = BitWriter_destroy(writer);
        this->bytes_out = dst->vptr->get_bytes_transferred(dst);
        return flush_ok && !src->vptr->has_error(src) && !dst->vptr->has_error(dst);
    }

    this->bytes_in += bytes_read;
    uint32_t ent = in_buf[0];

    size_t idx_in = 1;
    while (true) {
        if (idx_in >= bytes_read) {
            bytes_read = src->vptr->read(src, in_buf, IO_BUFFER_SIZE);
            if (bytes_read == 0) break;
            this->bytes_in += bytes_read;
            idx_in = 0;
        }

        uint32_t ch = in_buf[idx_in++];
        uint32_t key = (ent << 8) | ch;
        uint32_t idx = key % HASH_SIZE;
        uint32_t step = 1 + (key % (HASH_SIZE - 1));
        bool found = false;

        while (this->codetab[idx] != -1) {
            if (this->htab[idx] == key) {
                ent = (uint32_t)this->codetab[idx];
                found = true;
                break;
            }
            idx = (idx + step) % HASH_SIZE;
        }

        if (!found) {
            if (!BitWriter_write(writer, ent, n_bits)) {
                BitWriter_destroy(writer);
                return false;
            }

            if (free_ent < (1U << this->max_bits)) {
                this->codetab[idx] = (int32_t)free_ent++;
                this->htab[idx] = key;
                if (free_ent > maxcode && n_bits < this->max_bits) {
                    if (!BitWriter_align(writer, n_bits)) {
                        BitWriter_destroy(writer);
                        return false;
                    }
                    n_bits++;
                    maxcode = (n_bits == this->max_bits) ? (1U << this->max_bits) : ((1U << n_bits) - 1);
                }
            } else if (this->block_mode && this->bytes_in >= checkpoint) {
                checkpoint = this->bytes_in + CHECK_GAP;
                uint64_t cur_out = dst->vptr->get_bytes_transferred(dst);
                double cur_ratio = (cur_out > 0) ? ((double)this->bytes_in / (double)cur_out) : 0.0;

                if (cur_ratio < prev_ratio) {
                    if (!BitWriter_write(writer, CLEAR_CODE, n_bits) || !BitWriter_align(writer, n_bits)) {
                        BitWriter_destroy(writer);
                        return false;
                    }
                    LzwCompressor_clear_table(this);
                    free_ent = FIRST_FREE_CODE;
                    n_bits = INIT_BITS;
                    maxcode = (1U << INIT_BITS) - 1;
                    prev_ratio = 0.0;
                } else {
                    prev_ratio = cur_ratio;
                }
            }
            ent = ch;
        }
    }

    if (!BitWriter_write(writer, ent, n_bits)) {
        BitWriter_destroy(writer);
        return false;
    }

    bool flush_ok = BitWriter_destroy(writer);
    this->bytes_out = dst->vptr->get_bytes_transferred(dst);
    return flush_ok && !dst->vptr->has_error(dst) && !src->vptr->has_error(src);
}

static uint64_t LzwCompressor_get_in(const ICodec *super) {
    return ((const LzwCompressor *)super)->bytes_in;
}

static uint64_t LzwCompressor_get_out(const ICodec *super) {
    return ((const LzwCompressor *)super)->bytes_out;
}

static void LzwCompressor_destroy(ICodec *super) {
    LzwCompressor *this = (LzwCompressor *)super;
    if (this) {
        free(this->codetab);
        free(this->htab);
        free(this);
    }
}

static const ICodecVtbl LzwCompressor_Vtbl = {
    .execute = LzwCompressor_execute,
    .get_bytes_in = LzwCompressor_get_in,
    .get_bytes_out = LzwCompressor_get_out,
    .destroy = LzwCompressor_destroy
};

ICodec *LzwCompressor_create(int max_bits) {
    LzwCompressor *this = (LzwCompressor *)calloc(1, sizeof(LzwCompressor));
    if (!this) return NULL;
    this->base.vptr = &LzwCompressor_Vtbl;
    this->max_bits = max_bits;
    this->block_mode = true;
    this->codetab = (int32_t *)malloc(HASH_SIZE * sizeof(int32_t));
    this->htab = (uint32_t *)malloc(HASH_SIZE * sizeof(uint32_t));
    if (!this->codetab || !this->htab) {
        LzwCompressor_destroy((ICodec *)this);
        return NULL;
    }
    return (ICodec *)this;
}
