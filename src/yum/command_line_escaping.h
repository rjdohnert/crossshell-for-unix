#ifndef YUM_COMMAND_LINE_ESCAPING_H
#define YUM_COMMAND_LINE_ESCAPING_H

#include "yum_context.h"

void AppendEscapedArgument(wchar_t* buffer, size_t bufferSize, const wchar_t* arg);

#endif
