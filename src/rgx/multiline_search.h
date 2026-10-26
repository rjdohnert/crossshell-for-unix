#ifndef RGX_MULTILINE_SEARCH_H
#define RGX_MULTILINE_SEARCH_H
#include "file_search.h"
void search_multiline(FILE* stream,const char* path,const RgxOptions* options,IMatcher* matcher,RgxPrinter* printer,bool multiple_targets);
#endif