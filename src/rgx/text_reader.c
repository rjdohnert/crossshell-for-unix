#include "text_reader.h"
#include "allocation.h"
#include "search_state.h"

static int reader_raw(TextReader* reader) {
    if (reader->prefix_index < reader->prefix_count) return reader->prefix[reader->prefix_index++];
    if (reader->block_index == reader->block_count) {
        reader->block_count = fread(reader->block, 1, sizeof(reader->block), reader->stream);
        reader->block_index = 0;
        if (!reader->block_count) return EOF;
    }
    return reader->block[reader->block_index++];
}

void reader_init(TextReader* reader, FILE* stream) {
    memset(reader, 0, sizeof(*reader)); reader->stream = stream;
    reader->prefix_count = fread(reader->prefix, 1, 3, stream);
    if (reader->prefix_count >= 2 && reader->prefix[0] == 0xFF && reader->prefix[1] == 0xFE) { reader->encoding = 1; reader->prefix_index = 2; }
    else if (reader->prefix_count >= 2 && reader->prefix[0] == 0xFE && reader->prefix[1] == 0xFF) { reader->encoding = 2; reader->prefix_index = 2; }
    else if (reader->prefix_count == 3 && memcmp(reader->prefix, "\xEF\xBB\xBF", 3) == 0) reader->prefix_index = 3;
}

static int reader_unit(TextReader* reader) {
    int first = reader_raw(reader), second;
    if (first == EOF) return EOF;
    second = reader_raw(reader);
    if (second == EOF) { fprintf(stderr, "rgx: truncated UTF-16 input\n"); rgx_had_error = true; return EOF; }
    return reader->encoding == 1 ? first | (second << 8) : (first << 8) | second;
}

static int reader_byte(TextReader* reader) {
    if (!reader->encoding) return reader_raw(reader);
    if (reader->pending_index < reader->pending_count) return reader->pending[reader->pending_index++];
    int unit = reader_unit(reader); if (unit == EOF) return EOF;
    uint32_t rune = (uint32_t)unit;
    if (unit >= 0xD800 && unit <= 0xDBFF) { int low = reader_unit(reader); if (low < 0xDC00 || low > 0xDFFF) { fprintf(stderr, "rgx: invalid UTF-16 surrogate pair\n"); rgx_had_error = true; return EOF; } rune = 0x10000 + (((uint32_t)unit - 0xD800) << 10) + ((uint32_t)low - 0xDC00); }
    else if (unit >= 0xDC00 && unit <= 0xDFFF) { rgx_had_error = true; return EOF; }
    if (rune < 128) { reader->pending[0] = (unsigned char)rune; reader->pending_count = 1; }
    else if (rune < 0x800) { reader->pending[0] = (unsigned char)(0xC0 | (rune >> 6)); reader->pending[1] = (unsigned char)(0x80 | (rune & 63)); reader->pending_count = 2; }
    else if (rune < 0x10000) { reader->pending[0] = (unsigned char)(0xE0 | (rune >> 12)); reader->pending[1] = (unsigned char)(0x80 | ((rune >> 6) & 63)); reader->pending[2] = (unsigned char)(0x80 | (rune & 63)); reader->pending_count = 3; }
    else { reader->pending[0] = (unsigned char)(0xF0 | (rune >> 18)); reader->pending[1] = (unsigned char)(0x80 | ((rune >> 12) & 63)); reader->pending[2] = (unsigned char)(0x80 | ((rune >> 6) & 63)); reader->pending[3] = (unsigned char)(0x80 | (rune & 63)); reader->pending_count = 4; }
    reader->pending_index = 1; return reader->pending[0];
}

bool reader_line(TextReader* reader, char** buffer, size_t* capacity, size_t* length) {
    reader->line_terminated=false;
    *length = 0; int byte;
    while ((byte = reader_byte(reader)) != EOF) {
        if (byte == '\n') {reader->line_terminated=true;return true;}
        if (*length == 64U * 1024U * 1024U) { fprintf(stderr, "rgx: line exceeds 64 MiB safety limit\n"); rgx_had_error = true; return false; }
        if (*length == *capacity) { *capacity = *capacity ? *capacity * 2 : 4096; *buffer = (char*)rgx_realloc_array(*buffer, *capacity, 1); }
        (*buffer)[(*length)++] = (char)byte;
    }
    return *length != 0;
}
