#ifndef YUM_PACKAGE_INSTALL_H
#define YUM_PACKAGE_INSTALL_H

#include "yum_context.h"

int Yum_Install(const YumContext* ctx, const wchar_t* const packages[], int count);
int Yum_LocalInstall(const YumContext* ctx, const wchar_t* const packages[], int count);
int Yum_Reinstall(const YumContext* ctx, const wchar_t* const packages[], int count);
int Yum_Downgrade(const YumContext* ctx, const wchar_t* const packages[], int count);

#endif
