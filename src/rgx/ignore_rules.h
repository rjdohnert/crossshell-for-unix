#ifndef RGX_IGNORE_RULES_H
#define RGX_IGNORE_RULES_H

#include "rgx_common.h"

typedef struct IgnoreRule {
    char* scope;
    char* pattern;
    bool negate, directory_only, anchored;
    int priority;
    struct IgnoreRule* next;
} IgnoreRule;

typedef struct LoadedScope {
    char* path;
    struct LoadedScope* next;
} LoadedScope;
typedef struct RgxWalker RgxWalker;
void ignore_load_ancestors(RgxWalker* walker, const char* directory);
void ignore_load_directory(RgxWalker* walker, const char* scope);
bool walker_ignored(RgxWalker* walker, const char* absolute, bool directory, const char* root);

#endif
