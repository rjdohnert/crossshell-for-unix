#include "package_upgrade.h"
#include "winget_dispatch.h"
#include "rpm_dispatch.h"
#include "windows_update_query.h"

int Yum_Upgrade(const YumContext* ctx, const wchar_t* const packages[], int count) {
    if (count <= 0) {
        wprintf(L"Loaded plugins: fastestmirror\nResolving system-wide package dependencies for upgrade...\n\n");
        const wchar_t* args[6];
        size_t n = 0;
        args[n++] = L"upgrade";
        args[n++] = L"--all";
        args[n++] = L"--include-unknown";
        if (ctx->assume_yes) args[n++] = L"--silent";

        return ExecuteWinget(ctx, args, n);
    }

    int overallStatus = 0;

    for (int i = 0; i < count; ++i) {
        const wchar_t* package = packages[i];

        if (Yum_IsRpmPackage(package)) {
            int ret = Yum_RpmInstall(ctx, &package, 1, true);
            if (ret != 0) overallStatus = ret;
            continue;
        }

        wprintf(L"Loaded plugins: fastestmirror\nUpgrading package: %ls\n\n", package);

        const wchar_t* args[6];
        size_t n = 0;
        args[n++] = L"upgrade";
        args[n++] = package;
        if (ctx->assume_yes) args[n++] = L"--silent";

        int res = ExecuteWinget(ctx, args, n);
        if (res != 0) overallStatus = res;
    }

    return overallStatus;
}

int Yum_CheckUpdate(const YumContext* ctx) {
    wprintf(L"Loaded plugins: fastestmirror, native-wua\n");
    wprintf(L"Checking for available application and system updates...\n\n");

    const wchar_t* args[] = { L"upgrade" };
    ExecuteWinget(ctx, args, 1);

    wprintf(L"\n");
    return Yum_UpdateWindows(ctx);
}
