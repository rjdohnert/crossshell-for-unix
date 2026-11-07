#ifndef CSPLIT_ENGINE_H
#define CSPLIT_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "output_file_manager.h"
#include "pattern.h"

/* Read input, apply ordered patterns, and write split pieces transactionally. */
bool CSplitEngine_run(FILE* input, Pattern* const* patterns, size_t pattern_count,
                      size_t max_lookbehind, OutputFileManager* output);

#endif
