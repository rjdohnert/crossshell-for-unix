#include "package_list.h"
#include "winget_dispatch.h"
#include "rpm_dispatch.h"

int Yum_List(const YumContext* ctx, const wchar_t* filter) {
    if (!filter || _wcsicmp(filter, L"all") == 0 || _wcsicmp(filter, L"installed") == 0) {
        wprintf(L"Loaded plugins: fastestmirror, native-rpm\n");
        wprintf(L"Installed Applications & Packages:\n\n");
        const wchar_t* args[] = { L"list" };
        int res = ExecuteWinget(ctx, args, 1);
        Yum_RpmList(ctx);
        return res;
    } else if (_wcsicmp(filter, L"updates") == 0) {
        wprintf(L"Loaded plugins: fastestmirror\nPackages with available upgrades:\n\n");
        const wchar_t* args[] = { L"upgrade" };
        return ExecuteWinget(ctx, args, 1);
    }

    const wchar_t* args[] = { L"list", filter };
    return ExecuteWinget(ctx, args, 2);
}
