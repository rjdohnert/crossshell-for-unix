#ifndef RGX_PATTERN_SET_H
#define RGX_PATTERN_SET_H

#include "matcher.h"
#include "options.h"

IMatcher* PatternSet_New(const RgxOptions* options, bool ignore_case);

#endif
