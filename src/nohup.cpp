/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>

// ============================================================================
// 1. RAII HANDLES & RESOURCE GUARDS
// ============================================================================

class ScopedFileHandle {
public:
    explicit ScopedFileHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}

    ~ScopedFileHandle() {
        Close();
    }

    ScopedFileHandle(const ScopedFileHandle&) = delete;
    ScopedFileHandle& operator=(const ScopedFileHandle&) = delete;

    ScopedFileHandle(ScopedFileHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }

    ScopedFileHandle& operator=(ScopedFileHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    HANDLE* Receive() { Close(); return &m_handle; }
    bool IsValid() const { return m_handle != INVALID_HANDLE_VALUE && m_handle != NULL; }

    void Close() {
        if (m_handle != INVALID_HANDLE_VALUE && m_handle != NULL) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

    void Reset(HANDLE handle = INVALID_HANDLE_VALUE) {
        Close();
        m_handle = handle;
    }

private:
    HANDLE m_handle;
};

class ScopedProcessHandle {
public:
    explicit ScopedProcessHandle(HANDLE handle = NULL) : m_handle(handle) {}

    ~ScopedProcessHandle() {
        Close();
    }

    ScopedProcessHandle(const ScopedProcessHandle&) = delete;
    ScopedProcessHandle& operator=(const ScopedProcessHandle&) = delete;

    ScopedProcessHandle(ScopedProcessHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = NULL;
    }

    ScopedProcessHandle& operator=(ScopedProcessHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = NULL;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    HANDLE* Receive() { Close(); return &m_handle; }
    bool IsValid() const { return m_handle != NULL && m_handle != INVALID_HANDLE_VALUE; }

    void Close() {
        if (m_handle != NULL && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = NULL;
        }
    }

private:
    HANDLE m_handle;
};

// ============================================================================
// 2. PATH RESOLVER & STRING UTILS
// ============================================================================

class NohupPathResolver {
public:
    static bool IsConsoleHandle(HANDLE h) {
        if (h == NULL || h == INVALID_HANDLE_VALUE) {
            return false;
        }
        DWORD mode = 0;
        return GetConsoleMode(h, &mode) != 0;
    }

    static HANDLE SafeDuplicateHandle(HANDLE hSource, HANDLE hFallback) {
        if (hSource == NULL || hSource == INVALID_HANDLE_VALUE) {
            return hFallback;
        }
        HANDLE hDup = INVALID_HANDLE_VALUE;
        if (DuplicateHandle(GetCurrentProcess(), hSource, GetCurrentProcess(), &hDup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
            return hDup;
        }
        return hFallback;
    }

    static std::wstring GetNohupOutputPath() {
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

    static bool EndsWithIcase(const std::wstring& str, const std::wstring& suffix) {
        if (str.length() < suffix.length()) return false;
        std::wstring sub = str.substr(str.length() - suffix.length());
        std::transform(sub.begin(), sub.end(), sub.begin(), ::towlower);
        std::wstring suff_lower = suffix;
        std::transform(suff_lower.begin(), suff_lower.end(), suff_lower.begin(), ::towlower);
        return sub == suff_lower;
    }

    static std::wstring QuoteArgument(const std::wstring& arg) {
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
};

// ============================================================================
// 3. OPTIONS PARSER
// ============================================================================

class NohupOptions {
public:
    bool showHelp = false;
    bool showVersion = false;
    int commandIndex = -1;

    bool Parse(int argc, wchar_t* argv[]) {
        if (argc < 2) {
            return false;
        }

        std::wstring first_arg = argv[1];
        if (first_arg == L"--help" || first_arg == L"-h" || first_arg == L"/?") {
            showHelp = true;
            return true;
        }
        if (first_arg == L"--version" || first_arg == L"-v") {
            showVersion = true;
            return true;
        }

        commandIndex = 1;
        return true;
    }

    void PrintHelp() const {
        std::wcout << L"NAME\n"
                  << L"    nohup - run a command immune to hangups, with output to a non-tty\n\n"
                  << L"SYNOPSIS\n"
                  << L"    nohup COMMAND [ARGUMENT...]\n"
                  << L"    nohup OPTION\n\n"
                  << L"DESCRIPTION\n"
                  << L"    Run COMMAND, ignoring hangup signals (Console Close, Ctrl+C, Ctrl+Break,\n"
                  << L"    Logoff, and Shutdown events).\n\n"
                  << L"    If standard input is a terminal/console, it is redirected from NUL.\n"
                  << L"    If standard output is a terminal/console, output is appended to 'nohup.out'.\n"
                  << L"    If 'nohup.out' cannot be written in current dir, output is appended to\n"
                  << L"    '%USERPROFILE%\\nohup.out' or '%TEMP%\\nohup.out'.\n"
                  << L"    If standard error is a terminal/console, it is redirected to standard output.\n\n"
                  << L"OPTIONS\n"
                  << L"    --help, -h, /?    Display this comprehensive help message and exit.\n"
                  << L"    --version, -v     Output version information and exit.\n\n"
                  << L"EXIT STATUS\n"
                  << L"    126               COMMAND was found but could not be invoked.\n"
                  << L"    127               COMMAND could not be found or an internal error occurred.\n"
                  << L"    Otherwise         The exit status of COMMAND.\n\n"
                  << L"EXAMPLES\n"
                  << L"    nohup my_script.bat\n"
                  << L"    nohup python long_job.py \"arg with spaces\" 100\n"
                  << L"    nohup ping 127.0.0.1 -t > custom_log.txt 2>&1\n";
    }

    void PrintVersion() const {
        std::wcout << L"nohup 1.0.0\n";
    }
};

// ============================================================================
// 4. DETACHED PROCESS LAUNCHER
// ============================================================================

class DetachedProcessLauncher {
public:
    static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
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

    static int Launch(int argc, wchar_t* argv[], const NohupOptions& options) {
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
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class NohupApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        NohupOptions options;
        if (!options.Parse(argc, argv)) {
            std::wcerr << L"nohup: missing operand\n"
                      << L"Try 'nohup --help' for more information.\n";
            return 127;
        }

        if (options.showHelp) {
            options.PrintHelp();
            return 0;
        }

        if (options.showVersion) {
            options.PrintVersion();
            return 0;
        }

        return DetachedProcessLauncher::Launch(argc, argv, options);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    NohupApplication app;
    return app.Run(argc, argv);
}