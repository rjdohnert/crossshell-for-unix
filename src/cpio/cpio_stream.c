#include "cpio_stream.h"

/* ========================================================================= */
/* RESILIENT STREAM & I/O SUBSYSTEM                                          */
/* ========================================================================= */

void ForceBinaryStream(FILE* fp) {
    if (fp) _setmode(_fileno(fp), _O_BINARY);
}

bool WriteAll(FILE* fp, const void* buffer, size_t size) {
    if (size == 0) return true;
    return fwrite(buffer, 1, size, fp) == size;
}

bool WriteZeroPadding(FILE* fp, size_t count) {
    static const char zeroes[4] = {0, 0, 0, 0};
    if (count == 0) return true;
    return WriteAll(fp, zeroes, count);
}

bool StreamSkip(FILE* fp, uint64_t count) {
    if (count == 0) return true;

    if (_fseeki64(fp, (__int64)count, SEEK_CUR) == 0) {
        return true;
    }

    char discard[BUFFER_CHUNK_SIZE];
    uint64_t left = count;
    while (left > 0) {
        size_t toRead = left < sizeof(discard) ? (size_t)left : sizeof(discard);
        size_t n = fread(discard, 1, toRead, fp);
        if (n == 0) return false;
        left -= n;
    }
    return true;
}

char* ReadStreamLine(FILE* fp, int delim) {
    size_t capacity = 256;
    size_t length = 0;
    char* buf = (char*)malloc(capacity);
    if (!buf) return NULL;

    int ch;
    while ((ch = fgetc(fp)) != EOF && ch != delim) {
        if (delim == '\n' && ch == '\r') continue;
        if (length + 1 >= capacity) {
            capacity *= 2;
            char* newBuf = (char*)realloc(buf, capacity);
            if (!newBuf) {
                free(buf);
                return NULL;
            }
            buf = newBuf;
        }
        buf[length++] = (char)ch;
    }

    if (ch == EOF && length == 0) {
        free(buf);
        return NULL;
    }

    buf[length] = '\0';
    return buf;
}
