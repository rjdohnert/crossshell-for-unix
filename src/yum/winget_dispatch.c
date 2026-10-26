#include "winget_dispatch.h"
#include "command_line_escaping.h"

static bool ResolveWingetPath(wchar_t* resolvedPath, size_t maxLen) {
    if (SearchPathW(NULL, L"winget.exe", NULL, (DWORD)maxLen, resolvedPath, NULL) > 0) {
        return true;
    }

    wchar_t localApp[MAX_PATH];
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH) > 0) {
        wchar_t candidate[MAX_PATH];
        swprintf_s(candidate, MAX_PATH, L"%ls\\Microsoft\\WindowsApps\\winget.exe", localApp);
        if (GetFileAttributesW(candidate) != INVALID_FILE_ATTRIBUTES) {
            wcsncpy_s(resolvedPath, maxLen, candidate, _TRUNCATE);
            return true;
        }
    }

    wchar_t userProf[MAX_PATH];
    if (GetEnvironmentVariableW(L"USERPROFILE", userProf, MAX_PATH) > 0) {
        wchar_t candidate[MAX_PATH];
        swprintf_s(candidate, MAX_PATH, L"%ls\\AppData\\Local\\Microsoft\\WindowsApps\\winget.exe", userProf);
        if (GetFileAttributesW(candidate) != INVALID_FILE_ATTRIBUTES) {
            wcsncpy_s(resolvedPath, maxLen, candidate, _TRUNCATE);
            return true;
        }
    }

    wchar_t progFiles[MAX_PATH];
    if (GetEnvironmentVariableW(L"ProgramFiles", progFiles, MAX_PATH) > 0) {
        wchar_t searchPattern[MAX_PATH];
        swprintf_s(searchPattern, MAX_PATH, L"%ls\\WindowsApps\\Microsoft.DesktopAppInstaller_*_x64__8wekyb3d8bbwe\\winget.exe", progFiles);
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(searchPattern, &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            wchar_t candidate[MAX_PATH];
            swprintf_s(candidate, MAX_PATH, L"%ls\\WindowsApps\\%ls\\winget.exe", progFiles, fd.cFileName);
            FindClose(hFind);
            if (GetFileAttributesW(candidate) != INVALID_FILE_ATTRIBUTES) {
                wcsncpy_s(resolvedPath, maxLen, candidate, _TRUNCATE);
                return true;
            }
        }
    }

    wcsncpy_s(resolvedPath, maxLen, L"winget.exe", _TRUNCATE);
    return false;
}

bool Yum_HasWinget(void) {
    wchar_t path[MAX_PATH];
    return ResolveWingetPath(path, MAX_PATH);
}

int ExecuteWinget(const YumContext* ctx, const wchar_t* const args[], size_t argCount) {
    wchar_t wingetExe[MAX_PATH] = { 0 };
    bool found = ResolveWingetPath(wingetExe, MAX_PATH);

    wchar_t cmdLine[MAX_CMDLINE] = { 0 };
    AppendEscapedArgument(cmdLine, MAX_CMDLINE, wingetExe);

    for (size_t i = 0; i < argCount; ++i) {
        if (args[i] && args[i][0] != L'\0') {
            AppendEscapedArgument(cmdLine, MAX_CMDLINE, args[i]);
        }
    }

    if (ctx->verbose) {
        wprintf(L"[DEBUG] Spawning: %ls\n", cmdLine);
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND && !found) {
            fwprintf(stderr, L"Error: Windows Package Manager (winget.exe) was not found.\n");
            fwprintf(stderr, L"Please install 'App Installer' from the Microsoft Store or GitHub,\n");
            fwprintf(stderr, L"or manage native RPM packages with yum using .rpm files.\n");
        } else {
            fwprintf(stderr, L"Error: Failed to dispatch winget.exe (Win32 Error: %lu)\n", err);
        }
        return (int)err;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int)exitCode;
}
