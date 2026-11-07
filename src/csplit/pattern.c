/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 * See the repository license file for the full license text.
 */

#include "pattern.h"

#include <stdlib.h>

Pattern* Pattern_create_line(size_t target_line, long long repeat_count) {
    Pattern* pattern = (Pattern*)calloc(1, sizeof(Pattern));
    if (!pattern) return NULL;
    pattern->type = PATTERN_LINE_NUMBER;
    pattern->target_line = target_line;
    pattern->repeat_count = repeat_count;
    return pattern;
}

Pattern* Pattern_create_regex(PatternType type, const char* expression,
                              long long offset, long long repeat_count) {
    if (type != PATTERN_REGEX_SPLIT && type != PATTERN_REGEX_SKIP) return NULL;

    Pattern* pattern = (Pattern*)calloc(1, sizeof(Pattern));
    if (!pattern) return NULL;
    pattern->regex = RegexEngine_compile(expression);
    if (!pattern->regex) {
        free(pattern);
        return NULL;
    }
    pattern->type = type;
    pattern->offset = offset;
    pattern->repeat_count = repeat_count;
    return pattern;
}

int Pattern_matches(const Pattern* pattern, const char* line, size_t line_number) {
    if (!pattern) return 0;
    if (pattern->type == PATTERN_LINE_NUMBER) {
        return line_number == pattern->target_line;
    }
    return RegexEngine_matches(pattern->regex, line);
}

void Pattern_destroy(Pattern* pattern) {
    if (!pattern) return;
    RegexEngine_destroy(pattern->regex);
    free(pattern);
}
