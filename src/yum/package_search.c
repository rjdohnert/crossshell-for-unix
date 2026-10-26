#include "package_search.h"
#include "winget_dispatch.h"

int Yum_Search(const YumContext* ctx, const wchar_t* const queries[], int count) {
    if (count <= 0) {
        fwprintf(stderr, L"Error: Missing search term.\n");
        return 1;
    }

    int overallStatus = 0;

    for (int i = 0; i < count; ++i) {
        const wchar_t* args[] = { L"search", queries[i] };
        int res = ExecuteWinget(ctx, args, 2);
        if (res != 0) overallStatus = res;
    }

    return overallStatus;
}
