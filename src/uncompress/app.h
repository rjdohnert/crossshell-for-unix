#ifndef APP_H
#define APP_H

#include "common.h"
#include "cli.h"

typedef struct UncompressApp UncompressApp;

UncompressApp* UncompressApp_Create(CliOptions* options);
void UncompressApp_Destroy(UncompressApp* self);
int UncompressApp_Run(UncompressApp* self);

#endif
