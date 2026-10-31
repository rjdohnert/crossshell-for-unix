#ifndef APP_H
#define APP_H

#include "common.h"

typedef struct {
    int max_bits;
    bool force;
    bool verbose;
    bool decompress;
    bool to_stdout;
    bool keep;
} AppConfig;

typedef struct {
    AppConfig config;
    wchar_t **files;
    int file_count;
} CompressApp;

CompressApp *CompressApp_create(void);
void CompressApp_destroy(CompressApp *this);
int CompressApp_run(CompressApp *this, int argc, wchar_t **argv);

#endif
