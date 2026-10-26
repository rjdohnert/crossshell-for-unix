#include "repository_list.h"
#include "winget_dispatch.h"

int Yum_RepoList(const YumContext* ctx) {
    const wchar_t* args[] = { L"source", L"list" };
    return ExecuteWinget(ctx, args, 2);
}
