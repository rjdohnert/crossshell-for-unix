#include "repository_add.h"
#include "winget_dispatch.h"

int Yum_RepoAdd(const YumContext* ctx, const wchar_t* name, const wchar_t* url) {
    const wchar_t* args[] = { L"source", L"add", L"--name", name, L"--arg", url };
    return ExecuteWinget(ctx, args, 5);
}
