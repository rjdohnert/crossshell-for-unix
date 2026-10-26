#ifndef RGX_FILE_SEARCH_H
#define RGX_FILE_SEARCH_H

#include "options.h"
#include "matcher.h"
#include "printer.h"

void SearchSingleFile(const char* filepath, const RgxOptions* opts, IMatcher* matcher, RgxPrinter* printer, bool multiple_targets);

#endif
