#ifndef RGX_PARALLEL_SEARCH_H
#define RGX_PARALLEL_SEARCH_H
#include "printer.h"
typedef struct ParallelSearch ParallelSearch;
ParallelSearch* parallel_search_new(const RgxOptions* options,RgxPrinter* printer,bool multiple_targets,size_t workers);
bool parallel_search_submit(ParallelSearch* pool,const char* path);
void parallel_search_finish(ParallelSearch* pool);
#endif