#ifndef RGX_MATCHER_H
#define RGX_MATCHER_H

#include "rgx_common.h"

typedef struct MatchSpan {
    size_t start;
    size_t length;
} MatchSpan;

typedef struct IMatcher IMatcher;
typedef struct IMatcherVTable {
    bool (*FindMatch)(IMatcher* self, const char* text, size_t text_len, size_t offset, MatchSpan* out_span);
    void (*Destroy)(IMatcher* self);
} IMatcherVTable;

struct IMatcher {
    const IMatcherVTable* vptr;
};

/* --- Literal Matcher (Boyer-Moore / Substring with Word boundary) --- */

#endif
