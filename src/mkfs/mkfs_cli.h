#ifndef MKFS_CLI_H
#define MKFS_CLI_H

#include "mkfs_types.h"

bool MkfsOptionsParse(int argc, wchar_t* argv[], MkfsOptions* options, MkfsError* error);
void MkfsPrintHelp(void);
void MkfsPrintVersion(void);

#endif
