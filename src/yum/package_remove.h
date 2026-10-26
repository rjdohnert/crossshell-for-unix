#ifndef YUM_PACKAGE_REMOVE_H
#define YUM_PACKAGE_REMOVE_H

#include "yum_context.h"

int Yum_Remove(const YumContext* ctx, const wchar_t* const packages[], int count);

#endif
