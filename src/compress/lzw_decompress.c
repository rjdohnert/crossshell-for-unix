#include "codec.h"
#include "bitio.h"

typedef struct {
    ICodec base;
    uint64_t bytes_in;
    uint64_t bytes_out;
    uint16_t *prefix;
    uint8_t *suffix;
    uint8_t *stack;
    uint8_t out_buf[IO_BUFFER_SIZE];
    size_t out_pos;
} LzwDecompressor;

static bool LzwDecompressor_flush_out(LzwDecompressor *this, IByteStream *dst) {
    if (this->out_pos > 0) {
        size_t written = dst->vptr->write(dst, this->out_buf, this->out_pos);
        if (written != this->out_pos) return false;
        this->bytes_out += written;
        this->out_pos = 0;
    }
    return true;
}

static bool LzwDecompressor_execute(ICodec *super, IByteStream *src, IByteStream *dst) {
    LzwDecompressor *this = (LzwDecompressor *)super;

    uint8_t header[3];
    if (src->vptr->read(src, header, 3) < 3) {
        fwprintf(stderr, L"compress: stdin: unexpected end of file reading header\n");
        return false;
    }

    if (header[0] != MAGIC_BYTE_0 || header[1] != MAGIC_BYTE_1) {
        fwprintf(stderr, L"compress: not in compressed format\n");
        return false;
    }

    int max_bits = header[2] & BITS_MASK;
    bool block_mode = (header[2] & BLOCK_MASK) != 0;

    if (max_bits < INIT_BITS || max_bits > DEFAULT_BITS) {
        fwprintf(stderr, L"compress: archive compressed with %d bits, can only handle %d bits\n", max_bits, DEFAULT_BITS);
        return false;
    }

    BitReader *reader = BitReader_create(src);
    if (!reader) return false;

    int n_bits = INIT_BITS;
    uint32_t maxcode = (1U << n_bits) - 1;
    uint32_t free_ent = block_mode ? FIRST_FREE_CODE : 256;

    for (size_t i = 0; i < 256; i++) {
        this->prefix[i] = 0;
        this->suffix[i] = (uint8_t)i;
    }

    uint32_t code = 0;
    if (!BitReader_read(reader, n_bits, &code)) {
        BitReader_destroy(reader);
        return !src->vptr->has_error(src);
    }

    uint32_t oldcode = code;
    uint8_t finchar = (uint8_t)oldcode;

    this->out_buf[this->out_pos++] = finchar;
    int stack_ptr = 0;

    while (BitReader_read(reader, n_bits, &code)) {
        if (code == CLEAR_CODE && block_mode) {
            for (size_t i = 0; i < 256; i++) {
                this->prefix[i] = 0;
                this->suffix[i] = (uint8_t)i;
            }
            free_ent = FIRST_FREE_CODE;
            n_bits = INIT_BITS;
            maxcode = (1U << n_bits) - 1;
            BitReader_align(reader);

            if (!BitReader_read(reader, n_bits, &code)) break;
            oldcode = code;
            finchar = (uint8_t)oldcode;
            if (this->out_pos >= IO_BUFFER_SIZE) {
                if (!LzwDecompressor_flush_out(this, dst)) {
                    BitReader_destroy(reader);
                    return false;
                }
            }
            this->out_buf[this->out_pos++] = finchar;
            continue;
        }

        uint32_t incode = code;
        stack_ptr = 0;

        if (code >= free_ent) {
            if (code > free_ent) {
                fwprintf(stderr, L"compress: corrupt input stream: invalid code %u >= %u\n", code, free_ent);
                BitReader_destroy(reader);
                return false;
            }
            this->stack[stack_ptr++] = finchar;
            code = oldcode;
        }

        while (code >= 256) {
            if (stack_ptr >= STACK_SIZE) {
                fwprintf(stderr, L"compress: fatal error: cyclic graph detected in dictionary\n");
                BitReader_destroy(reader);
                return false;
            }
            this->stack[stack_ptr++] = this->suffix[code];
            code = this->prefix[code];
        }

        finchar = this->suffix[code];
        if (stack_ptr >= STACK_SIZE) {
            fwprintf(stderr, L"compress: fatal error: dictionary stack exhausted\n");
            BitReader_destroy(reader);
            return false;
        }
        this->stack[stack_ptr++] = finchar;

        while (stack_ptr > 0) {
            if (this->out_pos >= IO_BUFFER_SIZE) {
                if (!LzwDecompressor_flush_out(this, dst)) {
                    BitReader_destroy(reader);
                    return false;
                }
            }
            this->out_buf[this->out_pos++] = this->stack[--stack_ptr];
        }

        if (free_ent < (1U << max_bits)) {
            this->prefix[free_ent] = (uint16_t)oldcode;
            this->suffix[free_ent] = finchar;
            free_ent++;
            if (free_ent > maxcode && n_bits < max_bits) {
                BitReader_align(reader);
                n_bits++;
                maxcode = (n_bits == max_bits) ? (1U << max_bits) : ((1U << n_bits) - 1);
            }
        }
        oldcode = incode;
    }

    if (!LzwDecompressor_flush_out(this, dst)) {
        BitReader_destroy(reader);
        return false;
    }

    BitReader_destroy(reader);
    this->bytes_in = src->vptr->get_bytes_transferred(src);
    return !src->vptr->has_error(src) && !dst->vptr->has_error(dst);
}

static uint64_t LzwDecompressor_get_in(const ICodec *super) {
    return ((const LzwDecompressor *)super)->bytes_in;
}

static uint64_t LzwDecompressor_get_out(const ICodec *super) {
    return ((const LzwDecompressor *)super)->bytes_out;
}

static void LzwDecompressor_destroy(ICodec *super) {
    LzwDecompressor *this = (LzwDecompressor *)super;
    if (this) {
        free(this->prefix);
        free(this->suffix);
        free(this->stack);
        free(this);
    }
}

static const ICodecVtbl LzwDecompressor_Vtbl = {
    .execute = LzwDecompressor_execute,
    .get_bytes_in = LzwDecompressor_get_in,
    .get_bytes_out = LzwDecompressor_get_out,
    .destroy = LzwDecompressor_destroy
};

ICodec *LzwDecompressor_create(void) {
    LzwDecompressor *this = (LzwDecompressor *)calloc(1, sizeof(LzwDecompressor));
    if (!this) return NULL;
    this->base.vptr = &LzwDecompressor_Vtbl;
    this->prefix = (uint16_t *)malloc(65536 * sizeof(uint16_t));
    this->suffix = (uint8_t *)malloc(65536 * sizeof(uint8_t));
    this->stack  = (uint8_t *)malloc(STACK_SIZE * sizeof(uint8_t));
    if (!this->prefix || !this->suffix || !this->stack) {
        LzwDecompressor_destroy((ICodec *)this);
        return NULL;
    }
    return (ICodec *)this;
}
