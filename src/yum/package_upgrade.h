#ifndef YUM_PACKAGE_UPGRADE_H
#define YUM_PACKAGE_UPGRADE_H

#include "yum_context.h"

int Yum_Upgrade(const YumContext* ctx, const wchar_t* const packages[], int count);
int Yum_CheckUpdate(const YumContext* ctx);

#endif
