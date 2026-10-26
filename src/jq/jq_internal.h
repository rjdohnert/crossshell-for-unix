#ifndef JQ_INTERNAL_H
#define JQ_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <io.h>
#include <fcntl.h>
#include <windows.h>

#define JQ_VERSION "1.7.1-win"

/* =========================================================================
 * Terminal Color Palette (ANSI Virtual Terminal Sequences)
 * ========================================================================= */

#define COLOR_RESET   "\x1b[0m"
#define COLOR_NULL    "\x1b[90m"       /* Bright Black / Gray */
#define COLOR_BOOL    "\x1b[33m"       /* Yellow */
#define COLOR_NUMBER  "\x1b[36m"       /* Cyan */
#define COLOR_STRING  "\x1b[32m"       /* Green */
#define COLOR_KEY     "\x1b[1;34m"     /* Bold Blue */
#define COLOR_BRACKET "\x1b[1;37m"     /* Bold White */

/* =========================================================================
 * OOP: JqValue (JSON AST Node)
 * ========================================================================= */

typedef enum {
    JQ_NULL,
    JQ_BOOL,
    JQ_NUMBER,
    JQ_STRING,
    JQ_ARRAY,
    JQ_OBJECT
} JqType;

typedef struct JqValue JqValue;

typedef struct JqObjectEntry {
    char *key;
    JqValue *val;
} JqObjectEntry;

typedef struct JqSerializer JqSerializer;

typedef struct JqValueVTable {
    void (*destroy)(JqValue *self);
    JqValue* (*clone)(const JqValue *self);
    void (*serialize)(const JqValue *self, JqSerializer *ser, int depth);
    bool (*is_truthy)(const JqValue *self);
} JqValueVTable;

struct JqValue {
    const JqValueVTable *vptr;
    JqType type;
    union {
        bool bool_val;
        double num_val;
        char *str_val;
        struct {
            JqValue **items;
            size_t count;
            size_t cap;
        } arr;
        struct {
            JqObjectEntry *entries;
            size_t count;
            size_t cap;
        } obj;
    } as;
};

/* Object Constructors */

/* VTable Methods */

/* =========================================================================
 * OOP: JqConfig (Command-line Configuration)
 * Holds all genuine jq qualifiers.
 * ========================================================================= */

typedef struct JqNamedArg {
    char name[128];
    JqValue *val;
} JqNamedArg;

typedef struct JqConfig {
    bool compact_output;     /* -c, --compact-output */
    bool null_input;         /* -n, --null-input */
    bool raw_input;          /* -R, --raw-input */
    bool raw_output;         /* -r, --raw-output */
    bool join_output;        /* -j, --join-output */
    bool ascii_output;       /* -a, --ascii-output */
    bool sort_keys;          /* -S, --sort-keys */
    bool color_output;       /* -C, --color-output */
    bool monochrome_output;  /* -M, --monochrome-output */
    bool tab_indent;         /* --tab */
    int  indent_spaces;      /* --indent <n> */
    bool unbuffered;         /* --unbuffered */
    bool exit_status;        /* -e, --exit-status */
    bool slurp;              /* -s, --slurp */
    bool stream;             /* --stream */
    bool seq;                /* --seq */
    char from_file[MAX_PATH];/* -f, --from-file <file> */
    char library_path[MAX_PATH]; /* -L <dir> */
    bool run_tests;          /* --run-tests */
    bool args_mode;          /* --args */
    bool jsonargs_mode;      /* --jsonargs */

    JqNamedArg named_args[64];
    int named_arg_count;

    char *filter_string;
    char **input_files;
    int input_file_count;

    /* Class Methods */
    void (*print_help)(void);
    void (*print_version)(void);
    bool (*parse_args)(struct JqConfig *self, int argc, char *argv[]);
} JqConfig;

/* =========================================================================
 * OOP: JqSerializer (Pretty-Printing, Coloring, Escaping)
 * ========================================================================= */

struct JqSerializer {
    const JqConfig *config;
    FILE *out;
    bool enable_color;

    void (*write_indent)(struct JqSerializer *self, int depth);
    void (*write_string)(struct JqSerializer *self, const char *str, bool is_key);
};

/* =========================================================================
 * OOP: JqParser (Recursive Descent JSON Lexer & Parser)
 * ========================================================================= */

typedef struct JqParser {
    const char *src;
    size_t pos;
    size_t len;

    /* Parser Methods */
    JqValue* (*parse_value)(struct JqParser *self);
} JqParser;

/* =========================================================================
 * OOP: Filter Execution Pipeline
 * ========================================================================= */

typedef struct JqResultSet {
    JqValue **items;
    size_t count;
    size_t cap;
} JqResultSet;

typedef struct JqFilter JqFilter;

typedef struct JqFilterVTable {
    void (*destroy)(JqFilter *self);
    JqResultSet (*execute)(JqFilter *self, const JqValue *input);
} JqFilterVTable;

struct JqFilter {
    const JqFilterVTable *vptr;
};

/* 1. Identity Filter '.' */
typedef struct JqIdentityFilter {
    JqFilter base;
} JqIdentityFilter;

/* 2. Key Access Filter '.foo' */
typedef struct JqKeyFilter {
    JqFilter base;
    char key[128];
} JqKeyFilter;

/* 3. Array Index Filter '.[N]' */
typedef struct JqIndexFilter {
    JqFilter base;
    int index;
} JqIndexFilter;

/* 4. Array/Object Iterator '.[]' */
typedef struct JqIteratorFilter {
    JqFilter base;
} JqIteratorFilter;

/* 5. Builtin Function: 'keys', 'length', 'type' */
typedef struct JqBuiltinFilter {
    JqFilter base;
    char name[32];
} JqBuiltinFilter;

/* 6. Pipeline Combinator: 'A | B' */
typedef struct JqPipelineFilter {
    JqFilter base;
    JqFilter *left;
    JqFilter *right;
} JqPipelineFilter;

/* Filter Compiler */

/* =========================================================================
 * OOP: JqEngine (Application Orchestrator)
 * ========================================================================= */

typedef struct JqEngine {
    JqConfig *config;
    JqSerializer *serializer;
    JqFilter *filter;
    bool has_truthy_output;

    /* Engine Methods */
    int (*run)(struct JqEngine *self);
    void (*process_value)(struct JqEngine *self, JqValue *val);
    char* (*read_stream)(FILE *stream);
} JqEngine;

/* =========================================================================
 * Application Entry Point
 * ========================================================================= */

/* Module interfaces */
JqValue *JqValue_New(JqType t);
JqValue *JqValue_NewNull(void);
JqValue *JqValue_NewBool(bool b);
JqValue *JqValue_NewNumber(double n);
JqValue *JqValue_NewString(const char *s);
JqValue *JqValue_NewArray(void);
JqValue *JqValue_NewObject(void);
void JqValue_ArrayAppend(JqValue *self, JqValue *item);
void JqValue_ObjectSet(JqValue *self, const char *key, JqValue *val);
JqValue *JqValue_ObjectGet(const JqValue *self, const char *key);
void JqConfig_print_version(void);
void JqConfig_print_help(void);
bool JqConfig_parse_args(JqConfig *self, int argc, char *argv[]);
JqConfig *JqConfig_New(void);
void JqSerializer_write_indent(JqSerializer *self, int depth);
void JqSerializer_write_string(JqSerializer *self, const char *str, bool is_key);
int compare_entries(const void *a, const void *b);
void JqValue_serialize_impl(const JqValue *self, JqSerializer *ser, int depth);
JqSerializer *JqSerializer_New(const JqConfig *cfg, FILE *out);
void JqParser_skip_whitespace(JqParser *self);
char* JqParser_parse_raw_string(JqParser *self);
JqValue* JqParser_parse_value_impl(JqParser *self);
JqParser *JqParser_New(const char *src);
JqResultSet JqResultSet_Init(void);
void JqResultSet_Append(JqResultSet *rs, JqValue *v);
void JqResultSet_Free(JqResultSet *rs);
JqResultSet JqIdentityFilter_exec(JqFilter *self, const JqValue *input);
void JqIdentityFilter_destroy(JqFilter *self);
JqFilter *JqIdentityFilter_New(void);
JqResultSet JqKeyFilter_exec(JqFilter *self, const JqValue *input);
void JqKeyFilter_destroy(JqFilter *self);
JqFilter *JqKeyFilter_New(const char *key);
JqResultSet JqIndexFilter_exec(JqFilter *self, const JqValue *input);
void JqIndexFilter_destroy(JqFilter *self);
JqFilter *JqIndexFilter_New(int index);
JqResultSet JqIteratorFilter_exec(JqFilter *self, const JqValue *input);
void JqIteratorFilter_destroy(JqFilter *self);
JqFilter *JqIteratorFilter_New(void);
JqResultSet JqBuiltinFilter_exec(JqFilter *self, const JqValue *input);
void JqBuiltinFilter_destroy(JqFilter *self);
JqFilter *JqBuiltinFilter_New(const char *name);
JqResultSet JqPipelineFilter_exec(JqFilter *self, const JqValue *input);
void JqPipelineFilter_destroy(JqFilter *self);
JqFilter *JqPipelineFilter_New(JqFilter *l, JqFilter *r);
JqFilter* JqFilter_CompileSub(const char **expr);
JqFilter* JqFilter_Compile(const char *expr);
char* JqEngine_read_stream_impl(FILE *stream);
void JqEngine_process_value_impl(JqEngine *self, JqValue *val);
int JqEngine_run_impl(JqEngine *self);
JqEngine *JqEngine_New(JqConfig *cfg);
void JqEngine_Destroy(JqEngine *self);

#endif
