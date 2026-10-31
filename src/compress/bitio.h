#ifndef BITIO_H
#define BITIO_H

#include "common.h"
#include "bytestream.h"

/* BitWriter: standard UNIX 8-token aligned bit packer */
typedef struct BitWriter BitWriter;

BitWriter *BitWriter_create(IByteStream *output);
bool BitWriter_write(BitWriter *this, uint32_t code, int n_bits);
bool BitWriter_align(BitWriter *this, int n_bits);
bool BitWriter_flush(BitWriter *this);
bool BitWriter_destroy(BitWriter *this);

/* BitReader: standard UNIX 8-token aligned bit unpacker */
typedef struct BitReader BitReader;

BitReader *BitReader_create(IByteStream *input);
bool BitReader_read(BitReader *this, int n_bits, uint32_t *out_code);
void BitReader_align(BitReader *this);
void BitReader_destroy(BitReader *this);

#endif
