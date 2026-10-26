#ifndef RGX_REGEX_MATCHER_H
#define RGX_REGEX_MATCHER_H

#include "matcher.h"

IMatcher* RegexMatcher_New(const char* pattern, bool ignore_case, bool word_boundary);
IMatcher* RegexMatcher_NewMode(const char* pattern,bool ignore_case,bool word_boundary,bool multiline,bool dotall);
size_t regex_matcher_node_count(const IMatcher* matcher);
size_t regex_matcher_state_count(const IMatcher* matcher);

#endif
