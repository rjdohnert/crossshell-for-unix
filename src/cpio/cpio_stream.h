#ifndef CPIO_STREAM_H
#define CPIO_STREAM_H

#include "cpio.h"

// Binary stream and I/O helpers.
void  ForceBinaryStream(FILE* fp);
bool  WriteAll(FILE* fp, const void* buffer, size_t size);
bool  WriteZeroPadding(FILE* fp, size_t count);
bool  StreamSkip(FILE* fp, uint64_t count);
char* ReadStreamLine(FILE* fp, int delim);

#endif // CPIO_STREAM_H
