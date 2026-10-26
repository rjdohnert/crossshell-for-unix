#ifndef YUM_RPM_DISPATCH_H
#define YUM_RPM_DISPATCH_H

#include "yum_context.h"

bool Yum_IsRpmPackage(const wchar_t* target);
int ExecuteRpm(const YumContext* ctx, const wchar_t* const args[], size_t argCount);
int Yum_RpmInstall(const YumContext* ctx, const wchar_t* const packages[], int count, bool upgrade);
int Yum_RpmRemove(const YumContext* ctx, const wchar_t* const packages[], int count);
int Yum_RpmInfo(const YumContext* ctx, const wchar_t* target);
int Yum_RpmList(const YumContext* ctx);

#endif
