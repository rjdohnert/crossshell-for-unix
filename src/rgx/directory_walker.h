#ifndef RGX_DIRECTORY_WALKER_H
#define RGX_DIRECTORY_WALKER_H

#include "options.h"
#include "matcher.h"
#include "printer.h"
#include "ignore_rules.h"
#include "parallel_search.h"

struct RgxWalker {
    const RgxOptions* opts; IMatcher* matcher; RgxPrinter* printer; bool multiple_targets;
    IgnoreRule *rules, *tail; size_t rule_bytes;
    LoadedScope* loaded;
    size_t scope_bytes;
    ParallelSearch* parallel;
    char* git_root;
};
RgxWalker* RgxWalker_New(const RgxOptions* opts, IMatcher* matcher, RgxPrinter* printer, bool multiple_targets);
void RgxWalker_Execute(RgxWalker* self, const char* target_path);
void RgxWalker_Free(RgxWalker* self);

#endif
