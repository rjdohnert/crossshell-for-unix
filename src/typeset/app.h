#ifndef APP_H
#define APP_H

#include "common.h"

typedef struct TypesetApp TypesetApp;

TypesetApp* TypesetApp_Create(void);
int TypesetApp_Run(TypesetApp* self, int argc, char** argv);
void TypesetApp_Destroy(TypesetApp* self);

#endif
