#include "repository_remove.h"
#include "winget_dispatch.h"

int Yum_RepoRemove(const YumContext* ctx, const wchar_t* name) {
    const wchar_t* args[] = { L"source", L"remove", L"--name", name };
    return ExecuteWinget(ctx, args, 4);
}
