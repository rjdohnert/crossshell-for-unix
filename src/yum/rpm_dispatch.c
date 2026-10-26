#include "rpm_dispatch.h"
#include "command_line_escaping.h"

static bool FindRpmExe(wchar_t* outPath, size_t outSize) {
    wchar_t modulePath[MAX_PATH] = { 0 };
    if (GetModuleFileNameW(NULL, modulePath, MAX_PATH)) {
        wchar_t* lastSlash = wcsrchr(modulePath, L'\\');
        if (lastSlash) {
            *(lastSlash + 1) = L'\0';
            wcsncat_s(modulePath, MAX_PATH, L"rpm.exe", _TRUNCATE);
            if (GetFileAttributesW(modulePath) != INVALID_FILE_ATTRIBUTES) {
                wcsncpy_s(outPath, outSize, modulePath, _TRUNCATE);
                return true;
            }
        }
    }

    if (SearchPathW(NULL, L"rpm.exe", NULL, (DWORD)outSize, outPath, NULL) > 0) {
        return true;
    }

    wcsncpy_s(outPath, outSize, L"rpm.exe", _TRUNCATE);
    return false;
}

bool Yum_IsRpmPackage(const wchar_t* target) {
    if (!target) return false;
    size_t len = wcslen(target);
    if (len >= 4 && _wcsicmp(target + len - 4, L".rpm") == 0) {
        return true;
    }
    // Also check if file exists and has .rpm
    DWORD attr = GetFileAttributesW(target);
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        const wchar_t* ext = wcsrchr(target, L'.');
        if (ext && _wcsicmp(ext, L".rpm") == 0) return true;
    }
    return false;
}

int ExecuteRpm(const YumContext* ctx, const wchar_t* const args[], size_t argCount) {
    wchar_t rpmExe[MAX_PATH] = { 0 };
    FindRpmExe(rpmExe, MAX_PATH);

    wchar_t cmdLine[MAX_CMDLINE] = { 0 };
    AppendEscapedArgument(cmdLine, MAX_CMDLINE, rpmExe);

    for (size_t i = 0; i < argCount; ++i) {
        AppendEscapedArgument(cmdLine, MAX_CMDLINE, args[i]);
    }

    if (ctx->verbose) {
        wprintf(L"[DEBUG] Spawning RPM: %ls\n", cmdLine);
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        DWORD err = GetLastError();
        fwprintf(stderr, L"Error: Failed to dispatch rpm.exe (Win32 Error: %lu)\n", err);
        return (int)err;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int)exitCode;
}

int Yum_RpmInstall(const YumContext* ctx, const wchar_t* const packages[], int count, bool upgrade) {
    if (count <= 0) {
        fwprintf(stderr, L"Error: No RPM packages specified for installation.\n");
        return 1;
    }

    wprintf(L"Loaded plugins: fastestmirror, native-rpm\n");
    wprintf(L"Preparing RPM transaction for %d package(s)...\n\n", count);

    const wchar_t* args[128];
    size_t n = 0;
    args[n++] = upgrade ? L"-Uvh" : L"-ivh";

    for (int i = 0; i < count && n < 127; ++i) {
        args[n++] = packages[i];
    }

    return ExecuteRpm(ctx, args, n);
}

int Yum_RpmRemove(const YumContext* ctx, const wchar_t* const packages[], int count) {
    if (count <= 0) {
        fwprintf(stderr, L"Error: No RPM packages specified for removal.\n");
        return 1;
    }

    wprintf(L"Loaded plugins: fastestmirror, native-rpm\n");
    wprintf(L"Preparing RPM erase transaction for %d package(s)...\n\n", count);

    const wchar_t* args[128];
    size_t n = 0;
    args[n++] = L"-e";

    for (int i = 0; i < count && n < 127; ++i) {
        args[n++] = packages[i];
    }

    return ExecuteRpm(ctx, args, n);
}

int Yum_RpmInfo(const YumContext* ctx, const wchar_t* target) {
    if (!target) return 1;

    const wchar_t* args[4];
    size_t n = 0;

    if (Yum_IsRpmPackage(target)) {
        args[n++] = L"-qip";
        args[n++] = target;
    } else {
        args[n++] = L"-qi";
        args[n++] = target;
    }

    return ExecuteRpm(ctx, args, n);
}

int Yum_RpmList(const YumContext* ctx) {
    const wchar_t* args[] = { L"-qa" };
    wprintf(L"\n--------------------------------------------------------------------------------\n");
    wprintf(L" Native RPM Database Installed Packages (rpm -qa):\n");
    wprintf(L"--------------------------------------------------------------------------------\n");
    return ExecuteRpm(ctx, args, 1);
}
