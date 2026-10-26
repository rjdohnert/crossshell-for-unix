#include "package_remove.h"
#include "winget_dispatch.h"
#include "rpm_dispatch.h"

int Yum_Remove(const YumContext* ctx, const wchar_t* const packages[], int count) {
    if (count <= 0) {
        fwprintf(stderr, L"Error: Need a package to remove/erase.\n");
        return 1;
    }

    int overallStatus = 0;

    for (int i = 0; i < count; ++i) {
        const wchar_t* package = packages[i];

        if (Yum_IsRpmPackage(package)) {
            int ret = Yum_RpmRemove(ctx, &package, 1);
            if (ret != 0) overallStatus = ret;
            continue;
        }

        wprintf(L"Loaded plugins: fastestmirror\nPreparing erase transaction: %ls\n\n", package);

        const wchar_t* args[6];
        size_t n = 0;
        args[n++] = L"uninstall";
        args[n++] = L"--exact";
        args[n++] = package;
        if (ctx->assume_yes) args[n++] = L"--silent";

        int res = ExecuteWinget(ctx, args, n);
        if (res != 0) {
            if (ctx->verbose) {
                wprintf(L"[INFO] Exact match failed for uninstall of '%ls', retrying with standard query...\n", package);
            }
            n = 0;
            args[n++] = L"uninstall";
            args[n++] = package;
            if (ctx->assume_yes) args[n++] = L"--silent";

            res = ExecuteWinget(ctx, args, n);
        }

        if (res != 0) overallStatus = res;
    }

    return overallStatus;
}
