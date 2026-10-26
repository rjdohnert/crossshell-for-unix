#ifndef RGX_PATH_CONVERSION_H
#define RGX_PATH_CONVERSION_H

#include "rgx_common.h"

wchar_t* path_to_wide(const char* path);
char* wide_to_utf8(const wchar_t* text);
char* path_join(const char* directory, const char* name);
char* absolute_utf8(const char* path);

#endif
