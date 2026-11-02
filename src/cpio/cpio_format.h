#ifndef CPIO_FORMAT_H
#define CPIO_FORMAT_H

#include "cpio.h"

// Format decoding/encoding helpers.
uint32_t ParseHex8(const char* p);
uint32_t ParseOctal(const char* p, int len);
size_t   CalcPadding4(size_t bytes);
size_t   CalcPadding2(size_t bytes);
uint32_t SynthesizePosixMode(const struct _stat64* st);

// Universal header reader: auto-detects and decodes SVR4 newc/crc, POSIX.1 odc,
// and Old Binary (little/big endian).
bool ReadCpioHeader(FILE* in, GenericCpioEntry* e);

#endif // CPIO_FORMAT_H
