#include "bitio.h"

/* ---------------- BitWriter ---------------- */

struct BitWriter {
    IByteStream *output;
    uint8_t packet[DEFAULT_BITS + 4];
    int bit_offset;
    bool error_flag;
};

BitWriter *BitWriter_create(IByteStream *output) {
    BitWriter *this = (BitWriter *)calloc(1, sizeof(BitWriter));
    if (!this) return NULL;
    this->output = output;
    return this;
}

bool BitWriter_write(BitWriter *this, uint32_t code, int n_bits) {
    if (this->error_flag) return false;

    int byte_idx = this->bit_offset >> 3;
    int bit_idx = this->bit_offset & 7;

    this->packet[byte_idx] |= (uint8_t)((code << bit_idx) & 0xFF);
    this->packet[byte_idx + 1] |= (uint8_t)((code >> (8 - bit_idx)) & 0xFF);
    if (bit_idx + n_bits > 16) {
        this->packet[byte_idx + 2] |= (uint8_t)((code >> (16 - bit_idx)) & 0xFF);
    }

    this->bit_offset += n_bits;

    if (this->bit_offset >= (n_bits << 3)) {
        size_t written = this->output->vptr->write(this->output, this->packet, n_bits);
        if (written != (size_t)n_bits) {
            this->error_flag = true;
            return false;
        }
        memset(this->packet, 0, sizeof(this->packet));
        this->bit_offset = 0;
    }
    return true;
}

bool BitWriter_align(BitWriter *this, int n_bits) {
    if (this->error_flag) return false;
    if (this->bit_offset > 0) {
        size_t bytes_to_write = (size_t)n_bits;
        size_t written = this->output->vptr->write(this->output, this->packet, bytes_to_write);
        if (written != bytes_to_write) {
            this->error_flag = true;
            return false;
        }
        memset(this->packet, 0, sizeof(this->packet));
        this->bit_offset = 0;
    }
    return true;
}

bool BitWriter_flush(BitWriter *this) {
    if (this->error_flag) return false;
    if (this->bit_offset > 0) {
        size_t bytes_to_write = (this->bit_offset + 7) >> 3;
        size_t written = this->output->vptr->write(this->output, this->packet, bytes_to_write);
        if (written != bytes_to_write) {
            this->error_flag = true;
            return false;
        }
        memset(this->packet, 0, sizeof(this->packet));
        this->bit_offset = 0;
    }
    return this->output->vptr->flush(this->output);
}

bool BitWriter_destroy(BitWriter *this) {
    if (!this) return true;
    bool ok = BitWriter_flush(this);
    if (this->error_flag) ok = false;
    free(this);
    return ok;
}

/* ---------------- BitReader ---------------- */

struct BitReader {
    IByteStream *input;
    uint8_t packet[DEFAULT_BITS + 4];
    int bit_offset;
    size_t packet_size;
    bool eof_flag;
    bool error_flag;
};

BitReader *BitReader_create(IByteStream *input) {
    BitReader *this = (BitReader *)calloc(1, sizeof(BitReader));
    if (!this) return NULL;
    this->input = input;
    return this;
}

bool BitReader_read(BitReader *this, int n_bits, uint32_t *out_code) {
    if (this->error_flag || this->eof_flag) return false;

    if (this->bit_offset + n_bits > ((int)this->packet_size << 3)) {
        this->packet_size = this->input->vptr->read(this->input, this->packet, n_bits);
        this->bit_offset = 0;
        if (this->packet_size == 0) {
            this->eof_flag = true;
            return false;
        }
        if (this->packet_size < (size_t)n_bits) {
            memset(this->packet + this->packet_size, 0, sizeof(this->packet) - this->packet_size);
        }
        if (this->bit_offset + n_bits > ((int)this->packet_size << 3)) {
            this->eof_flag = true;
            return false;
        }
    }

    int byte_idx = this->bit_offset >> 3;
    int bit_idx = this->bit_offset & 7;

    uint32_t code = ((uint32_t)this->packet[byte_idx]) >> bit_idx;
    if (bit_idx + n_bits > 8) {
        code |= ((uint32_t)this->packet[byte_idx + 1]) << (8 - bit_idx);
    }
    if (bit_idx + n_bits > 16) {
        code |= ((uint32_t)this->packet[byte_idx + 2]) << (16 - bit_idx);
    }

    code &= (1U << n_bits) - 1;
    this->bit_offset += n_bits;
    *out_code = code;
    return true;
}

void BitReader_align(BitReader *this) {
    this->packet_size = 0;
    this->bit_offset = 0;
}

void BitReader_destroy(BitReader *this) {
    free(this);
}
