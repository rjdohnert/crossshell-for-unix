#ifndef RGX_ALLOCATION_H
#define RGX_ALLOCATION_H

#include "rgx_common.h"

void* rgx_calloc(size_t count, size_t size);
void* rgx_malloc(size_t size);
char* rgx_strdup(const char* text);
void* rgx_realloc_array(void* pointer, size_t count, size_t size);

#endif
