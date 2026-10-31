#ifndef CLI_H
#define CLI_H

#include "common.h"

typedef struct CliOptions {
    bool      toStdout;
    bool      force;
    bool      keep;
    bool      verbose;
    bool      quiet;
    wchar_t** fileList;
    int       fileCount;
    int       fileCapacity;
} CliOptions;

CliOptions* CliOptions_Create(int argc, wchar_t* argv[]);
void CliOptions_Destroy(CliOptions* self);

#endif
