#ifndef RGX_LITERAL_MATCHER_H
#define RGX_LITERAL_MATCHER_H

#include "matcher.h"

IMatcher* LiteralMatcher_New(const char* pattern, bool ignore_case, bool word_boundary);
void literal_matcher_set_full_line(IMatcher* matcher, bool full_line);
void literal_matcher_set_multiline(IMatcher* matcher,bool multiline);

#endif
