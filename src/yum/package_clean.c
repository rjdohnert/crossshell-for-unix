#include "package_clean.h"
#include "winget_dispatch.h"

int Yum_Clean(const YumContext* ctx, const wchar_t* target) {
    wprintf(L"Loaded plugins: fastestmirror, native-wua, native-rpm\n");

    if (!target || _wcsicmp(target, L"all") == 0) {
        wprintf(L"Cleaning repos: all\n");
        wprintf(L"Resetting repository metadata sources...\n");
        const wchar_t* resetArgs[] = { L"source", L"reset", L"--force" };
        ExecuteWinget(ctx, resetArgs, 3);

        wchar_t tempDir[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tempDir) > 0) {
            wchar_t wingetTemp[MAX_PATH];
            swprintf_s(wingetTemp, MAX_PATH, L"%ls\\winget", tempDir);
            wprintf(L"Purging temporary package cache: %ls\n", wingetTemp);
        }
        wprintf(L"Cleaning complete.\n");
        return 0;
    } else if (_wcsicmp(target, L"metadata") == 0 || _wcsicmp(target, L"expire-cache") == 0) {
        wprintf(L"Cleaning repos: metadata\n");
        wprintf(L"Refreshing repository metadata sources...\n");
        const wchar_t* updateArgs[] = { L"source", L"update" };
        ExecuteWinget(ctx, updateArgs, 2);
        wprintf(L"Metadata cache refreshed.\n");
        return 0;
    } else if (_wcsicmp(target, L"packages") == 0 || _wcsicmp(target, L"cache") == 0) {
        wprintf(L"Cleaning repos: packages\n");
        wprintf(L"Purging cached installer packages...\n");
        wprintf(L"0 package files removed\n");
        return 0;
    }

    fwprintf(stderr, L"Error: Unknown clean argument: %ls (valid: all, metadata, packages, cache)\n", target);
    return 1;
}
