#ifndef YUM_PACKAGE_INFO_H
#define YUM_PACKAGE_INFO_H

#include "yum_context.h"

int Yum_Info(const YumContext* ctx, const wchar_t* const packages[], int count);

#endif
