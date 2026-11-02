#ifndef CPIO_ENGINE_H
#define CPIO_ENGINE_H

#include "cpio.h"

// The four operational modes.
int ExecuteCreate(const CpioConfig* cfg);
int ExecuteExtractOrList(const CpioConfig* cfg);
int ExecutePassThrough(const CpioConfig* cfg);

#endif // CPIO_ENGINE_H
