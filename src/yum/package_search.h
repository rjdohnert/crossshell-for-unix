#ifndef YUM_PACKAGE_SEARCH_H
#define YUM_PACKAGE_SEARCH_H

#include "yum_context.h"

int Yum_Search(const YumContext* ctx, const wchar_t* const queries[], int count);

#endif
