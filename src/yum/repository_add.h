#ifndef YUM_REPOSITORY_ADD_H
#define YUM_REPOSITORY_ADD_H

#include "yum_context.h"

int Yum_RepoAdd(const YumContext* ctx, const wchar_t* name, const wchar_t* url);

#endif
