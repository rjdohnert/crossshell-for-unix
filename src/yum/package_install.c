#include "package_install.h"
#include "winget_dispatch.h"
#include "rpm_dispatch.h"

int Yum_Install(const YumContext* ctx, const wchar_t* const packages[], int count) {
    if (count <= 0) {
        fwprintf(stderr, L"Error: Need a package or target to install.\n");
        return 1;
    }

    int overallStatus = 0;

    for (int i = 0; i < count; ++i) {
        const wchar_t* package = packages[i];

        if (Yum_IsRpmPackage(package)) {
            int ret = Yum_RpmInstall(ctx, &package, 1, false);
            if (ret != 0) overallStatus = ret;
            continue;
        }

        wprintf(L"Loaded plugins: fastestmirror\nResolving dependencies for: %ls\n\n", package);

        const wchar_t* args[8];
        size_t n = 0;
        args[n++] = L"install";
        args[n++] = L"--exact";
        args[n++] = package;
        args[n++] = L"--accept-package-agreements";
        args[n++] = L"--accept-source-agreements";
        if (ctx->assume_yes) args[n++] = L"--silent";

        int res = ExecuteWinget(ctx, args, n);
        if (res != 0) {
            if (ctx->verbose) {
                wprintf(L"[INFO] Exact match failed for '%ls', retrying with standard resolution...\n", package);
            }
            n = 0;
            args[n++] = L"install";
            args[n++] = package;
            args[n++] = L"--accept-package-agreements";
            args[n++] = L"--accept-source-agreements";
            if (ctx->assume_yes) args[n++] = L"--silent";

            res = ExecuteWinget(ctx, args, n);
        }

        if (res != 0) overallStatus = res;
    }

    return overallStatus;
}

int Yum_LocalInstall(const YumContext* ctx, const wchar_t* const packages[], int count) {
    if (count <= 0) {
        fwprintf(stderr, L"Error: Need at least one .rpm package file to localinstall.\n");
        return 1;
    }
    return Yum_RpmInstall(ctx, packages, count, false);
}

int Yum_Reinstall(const YumContext* ctx, const wchar_t* const packages[], int count) {
    if (count <= 0) {
        fwprintf(stderr, L"Error: Need a package to reinstall.\n");
        return 1;
    }

    int overallStatus = 0;

    for (int i = 0; i < count; ++i) {
        const wchar_t* package = packages[i];
        if (Yum_IsRpmPackage(package)) {
            const wchar_t* rpmArgs[] = { L"-Uvh", L"--replacepkgs", package };
            int ret = ExecuteRpm(ctx, rpmArgs, 3);
            if (ret != 0) overallStatus = ret;
        } else {
            wprintf(L"Loaded plugins: fastestmirror\nReinstalling package: %ls\n\n", package);
            const wchar_t* args[8];
            size_t n = 0;
            args[n++] = L"install";
            args[n++] = L"--force";
            args[n++] = package;
            args[n++] = L"--accept-package-agreements";
            args[n++] = L"--accept-source-agreements";
            if (ctx->assume_yes) args[n++] = L"--silent";

            int res = ExecuteWinget(ctx, args, n);
            if (res != 0) overallStatus = res;
        }
    }

    return overallStatus;
}

int Yum_Downgrade(const YumContext* ctx, const wchar_t* const packages[], int count) {
    if (count <= 0) {
        fwprintf(stderr, L"Error: Need a package to downgrade.\n");
        return 1;
    }

    int overallStatus = 0;

    for (int i = 0; i < count; ++i) {
        const wchar_t* package = packages[i];
        if (Yum_IsRpmPackage(package)) {
            const wchar_t* rpmArgs[] = { L"-Uvh", L"--oldpackage", package };
            int ret = ExecuteRpm(ctx, rpmArgs, 3);
            if (ret != 0) overallStatus = ret;
        } else {
            wprintf(L"Loaded plugins: fastestmirror\nDowngrading package: %ls\n\n", package);
            const wchar_t* args[8];
            size_t n = 0;
            args[n++] = L"install";
            args[n++] = L"--force";
            args[n++] = package;
            args[n++] = L"--accept-package-agreements";
            args[n++] = L"--accept-source-agreements";
            if (ctx->assume_yes) args[n++] = L"--silent";

            int res = ExecuteWinget(ctx, args, n);
            if (res != 0) overallStatus = res;
        }
    }

    return overallStatus;
}
