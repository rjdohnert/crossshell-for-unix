#include "search_state.h"

RGX_THREAD_LOCAL bool rgx_had_error = false;
RGX_THREAD_LOCAL bool rgx_match_error = false;
RGX_THREAD_LOCAL bool rgx_found = false;
RGX_THREAD_LOCAL uint64_t rgx_search_work = 0;
