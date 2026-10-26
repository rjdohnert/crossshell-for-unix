#include "package_info.h"
#include "winget_dispatch.h"
#include "rpm_dispatch.h"

int Yum_Info(const YumContext* ctx, const wchar_t* const packages[], int count) {
    if (count <= 0) {
        fwprintf(stderr, L"Error: Missing package name for info.\n");
        return 1;
    }

    int overallStatus = 0;

    for (int i = 0; i < count; ++i) {
        const wchar_t* package = packages[i];

        if (Yum_IsRpmPackage(package)) {
            int ret = Yum_RpmInfo(ctx, package);
            if (ret != 0) overallStatus = ret;
            continue;
        }

        const wchar_t* args[4];
        args[0] = L"show";
        args[1] = L"--exact";
        args[2] = package;

        int res = ExecuteWinget(ctx, args, 3);
        if (res != 0) {
            args[0] = L"show";
            args[1] = package;
            res = ExecuteWinget(ctx, args, 2);
        }

        if (res != 0) overallStatus = res;
    }

    return overallStatus;
}
