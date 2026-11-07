#ifndef CSPLIT_PATTERN_H
#define CSPLIT_PATTERN_H

#include <stddef.h>

#include "regex_engine.h"

typedef enum {
    PATTERN_LINE_NUMBER,
    PATTERN_REGEX_SPLIT,
    PATTERN_REGEX_SKIP
} PatternType;

typedef struct Pattern {
    PatternType type;
    size_t target_line;
    long long offset;
    long long repeat_count;
    CompiledRegex* regex;
} Pattern;

/* Create a pattern that splits before a specified one-based input line. */
Pattern* Pattern_create_line(size_t target_line, long long repeat_count);

/* Compile a regex split/skip pattern and retain its offset and repeat count. */
Pattern* Pattern_create_regex(PatternType type, const char* expression,
                              long long offset, long long repeat_count);

/* Match a pattern against one normalized line; -1 reports a matching error. */
int Pattern_matches(const Pattern* pattern, const char* line, size_t line_number);

/* Release a pattern and its compiled regex, if any. */
void Pattern_destroy(Pattern* pattern);

#endif
