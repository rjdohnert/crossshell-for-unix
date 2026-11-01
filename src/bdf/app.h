#ifndef APP_H
#define APP_H

#include "common.h"

typedef struct BdfApplication BdfApplication;

BdfApplication* BdfApplication_Create(void);
int BdfApplication_Run(BdfApplication* self, int argc, char** argv);
void BdfApplication_Destroy(BdfApplication* app);

#endif
