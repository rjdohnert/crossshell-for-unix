#ifndef RGX_TEXT_READER_H
#define RGX_TEXT_READER_H

#include "rgx_common.h"

typedef struct {
	FILE* stream;
	unsigned char prefix[3], pending[4], block[65536];
	size_t prefix_count, prefix_index, pending_count, pending_index;
	size_t block_count, block_index;
	int encoding;
	bool line_terminated;
} TextReader;

void reader_init(TextReader* reader, FILE* stream);
bool reader_line(TextReader* reader, char** buffer, size_t* capacity, size_t* length);

#endif
