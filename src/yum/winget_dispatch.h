#ifndef YUM_WINGET_DISPATCH_H
#define YUM_WINGET_DISPATCH_H

#include "yum_context.h"

int ExecuteWinget(const YumContext* ctx, const wchar_t* const args[], size_t argCount);
bool Yum_HasWinget(void);

#endif
