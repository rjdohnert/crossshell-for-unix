#ifndef RGX_OPTIONS_H
#define RGX_OPTIONS_H

#include "rgx_common.h"

typedef enum {
    COLOR_NEVER,
    COLOR_AUTO,
    COLOR_ALWAYS
} ColorMode;

typedef struct RgxOptions {
    char** patterns;
    size_t pattern_count;
    char** paths;
    size_t path_count;

    /* Match qualifiers */
    bool fixed_strings;      /* -F, --fixed-strings */
    bool ignore_case;        /* -i, --ignore-case */
    bool smart_case;         /* -S, --smart-case */
    bool case_sensitive;     /* -s, --case-sensitive */
    bool invert_match;       /* -v, --invert-match */
    bool word_regexp;        /* -w, --word-regexp */

    /* Output qualifiers */
    bool show_line_numbers;  /* -n, --line-number */
    bool count_only;         /* -c, --count */
    bool files_with_matches; /* -l, --files-with-matches */
    bool files_without_match;/* --files-without-match */
    bool only_matching;      /* -o, --only-matching */
    bool quiet;              /* -q, --quiet */
    ColorMode color_mode;    /* --color <when> */

    /* Context qualifiers */
    size_t before_context;   /* -B, --before-context */
    size_t after_context;    /* -A, --after-context */
    size_t max_count;        /* -m, --max-count (0 = unlimited) */

    /* Traversal qualifiers */
    size_t max_depth;        /* --max-depth (0 = unlimited) */
    bool search_hidden;      /* --hidden */
    bool no_ignore;          /* --no-ignore */
    bool text;
    bool crlf;
    bool line_regexp;
    bool max_count_set;
    bool max_depth_set;
    bool files_only;
    bool count_matches;
    int filename_mode;
    char** globs;
    size_t glob_count;
    bool pattern_source_set;
    size_t option_string_bytes;
    bool json, byte_offset, column, null_paths, heading, stats;
    bool multiline, multiline_dotall, no_require_git, type_list;
    bool effective_ignore_case;
    size_t threads;
    char* replacement;
    char** types; size_t type_count;
    char** type_not; size_t type_not_count;
    char** type_add; size_t type_add_count;
    char** type_clear; size_t type_clear_count;
} RgxOptions;

RgxOptions* RgxOptions_New(void);
void RgxOptions_Free(RgxOptions* self);

#endif
