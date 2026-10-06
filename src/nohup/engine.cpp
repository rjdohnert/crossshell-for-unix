#include "engine.hpp"

// ============================================================================
// NohupPathResolver Implementation
// ============================================================================

bool NohupPathResolver::IsConsoleHandle(HANDLE h) {
    if (h == NULL || h == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD mode = 0;
    return GetConsoleMode(h, &mode) != 0;
}

HANDLE NohupPathResolver::SafeDuplicateHandle(HANDLE hSource, HANDLE hFallback) {
    if (hSource == NULL || hSource == INVALID_HANDLE_VALUE) {
        return hFallback;
    }
    HANDLE hDup = INVALID_HANDLE_VALUE;
    if (DuplicateHandle(GetCurrentProcess(), hSource, GetCurrentProcess(), &hDup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
        return hDup;
    }
    return hFallback;
}

std::wstring NohupPathResolver::GetNohupOutputPath() {
    // 1. Current working directory
    HANDLE hFile = CreateFileW(
        L"nohup.out",
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
        return L"nohup.out";
    }

    // 2. User Profile Directory (%USERPROFILE%\nohup.out)
    wchar_t userProfile[MAX_PATH];
    DWORD len = GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::wstring path = std::wstring(userProfile) + L"\\nohup.out";
        hFile = CreateFileW(
            path.c_str(),
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
            return path;
        }
    }

    // 3. Temp Directory (%TEMP%\nohup.out)
    wchar_t tempPath[MAX_PATH];
    len = GetTempPathW(MAX_PATH, tempPath);
    if (len > 0 && len < MAX_PATH) {
        return std::wstring(tempPath) + L"nohup.out";
    }

    return L"nohup.out";
}

bool NohupPathResolver::EndsWithIcase(const std::wstring& str, const std::wstring& suffix) {
    if (str.length() < suffix.length()) return false;
    std::wstring sub = str.substr(str.length() - suffix.length());
    std::transform(sub.begin(), sub.end(), sub.begin(), ::towlower);
    std::wstring suff_lower = suffix;
    std::transform(suff_lower.begin(), suff_lower.end(), suff_lower.begin(), ::towlower);
    return sub == suff_lower;
}

std::wstring NohupPathResolver::QuoteArgument(const std::wstring& arg) {
    if (arg.empty()) {
        return L"\"\"";
    }

    bool needs_quotes = false;
    for (wchar_t c : arg) {
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\v' || c == L'"') {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes) {
        return arg;
    }

    std::wstring result = L"\"";
    size_t i = 0;
    while (i < arg.length()) {
        size_t backslash_count = 0;
        while (i < arg.length() && arg[i] == L'\\') {
            backslash_count++;
            i++;
        }

        if (i == arg.length()) {
            result.append(backslash_count * 2, L'\\');
            break;
        } else if (arg[i] == L'"') {
            result.append(backslash_count * 2 + 1, L'\\');
            result.push_back(L'"');
            i++;
        } else {
            result.append(backslash_count, L'\\');
            result.push_back(arg[i]);
            i++;
        }
    }
    result.push_back(L'"');
    return result;
}

// ============================================================================
// DetachedProcessLauncher Implementation
// ============================================================================

BOOL WINAPI DetachedProcessLauncher::ConsoleCtrlHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            return TRUE;
        default:
            return FALSE;
    }
}

int DetachedProcessLauncher::Launch(int argc, wchar_t* argv[], const NohupOptions& options) {
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    std::wstring raw_command = argv[options.commandIndex];
    std::wstring target_cmdline;

    bool is_batch = NohupPathResolver::EndsWithIcase(raw_command, L".bat") || 
                    NohupPathResolver::EndsWithIcase(raw_command, L".cmd");

    if (is_batch) {
        target_cmdline = L"cmd.exe /c ";
        for (int i = options.commandIndex; i < argc; ++i) {
            if (i > options.commandIndex) target_cmdline += L" ";
            target_cmdline += NohupPathResolver::QuoteArgument(argv[i]);
        }
    } else {
        for (int i = options.commandIndex; i < argc; ++i) {
            if (i > options.commandIndex) target_cmdline += L" ";
            target_cmdline += NohupPathResolver::QuoteArgument(argv[i]);
        }
    }

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    HANDLE hStdIn  = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);

    ScopedFileHandle hInputFile;
    ScopedFileHandle hOutputFile;
    ScopedFileHandle hErrFile;

    // 1. Handle STDIN
    if (NohupPathResolver::IsConsoleHandle(hStdIn)) {
        HANDLE hNul = CreateFileW(
            L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
        );
        hInputFile.Reset(hNul);
    } else {
        hInputFile.Reset(NohupPathResolver::SafeDuplicateHandle(hStdIn, INVALID_HANDLE_VALUE));
    }

    // 2. Handle STDOUT
    if (NohupPathResolver::IsConsoleHandle(hStdOut)) {
        std::wstring outputPath = NohupPathResolver::GetNohupOutputPath();
        HANDLE hOut = CreateFileW(
            outputPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
        );

        if (hOut == INVALID_HANDLE_VALUE) {
            std::wcerr << L"nohup: failed to open output file\n";
            return 127;
        }

        hOutputFile.Reset(hOut);
        std::wcerr << L"nohup: appending output to " << outputPath << L"\n";
    } else {
        hOutputFile.Reset(NohupPathResolver::SafeDuplicateHandle(hStdOut, INVALID_HANDLE_VALUE));
    }

    // 3. Handle STDERR
    if (NohupPathResolver::IsConsoleHandle(hStdErr)) {
        hErrFile.Reset(NohupPathResolver::SafeDuplicateHandle(hOutputFile.Get(), INVALID_HANDLE_VALUE));
    } else {
        hErrFile.Reset(NohupPathResolver::SafeDuplicateHandle(hStdErr, INVALID_HANDLE_VALUE));
    }

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = hInputFile.Get();
    si.hStdOutput = hOutputFile.Get();
    si.hStdError  = hErrFile.Get();

    PROCESS_INFORMATION pi = {};
    DWORD creationFlags = DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP;

    std::vector<wchar_t> cmdBuffer(target_cmdline.begin(), target_cmdline.end());
    cmdBuffer.push_back(L'\0');

    BOOL success = CreateProcessW(
        NULL,
        cmdBuffer.data(),
        NULL,
        NULL,
        TRUE,
        creationFlags,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (!success && !is_batch) {
        std::wstring fallback_cmdline = L"cmd.exe /c " + target_cmdline;
        std::vector<wchar_t> fallbackBuffer(fallback_cmdline.begin(), fallback_cmdline.end());
        fallbackBuffer.push_back(L'\0');

        success = CreateProcessW(
            NULL,
            fallbackBuffer.data(),
            NULL,
            NULL,
            TRUE,
            creationFlags,
            NULL,
            NULL,
            &si,
            &pi
        );
    }

    DWORD exitCode = 127;

    if (!success) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            std::wcerr << L"nohup: cannot run command '" << argv[options.commandIndex] << L"': No such file or directory\n";
            exitCode = 127;
        } else if (err == ERROR_ACCESS_DENIED || err == ERROR_ELEVATION_REQUIRED) {
            std::wcerr << L"nohup: cannot run command '" << argv[options.commandIndex] << L"': Permission denied\n";
            exitCode = 126;
        } else {
            std::wcerr << L"nohup: failed to execute command '" << argv[options.commandIndex] << L"': Error code " << err << L"\n";
            exitCode = 126;
        }
    } else {
        ScopedProcessHandle hProcess(pi.hProcess);
        ScopedProcessHandle hThread(pi.hThread);

        WaitForSingleObject(hProcess.Get(), INFINITE);
        GetExitCodeProcess(hProcess.Get(), &exitCode);
    }

    return static_cast<int>(exitCode);
}
