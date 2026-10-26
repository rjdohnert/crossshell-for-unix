#ifndef RGX_SEARCH_STATE_H
#define RGX_SEARCH_STATE_H

#include "rgx_common.h"

#define RGX_THREAD_LOCAL __declspec(thread)
extern RGX_THREAD_LOCAL bool rgx_had_error;
extern RGX_THREAD_LOCAL bool rgx_match_error;
extern RGX_THREAD_LOCAL bool rgx_found;
extern RGX_THREAD_LOCAL uint64_t rgx_search_work;

#endif
