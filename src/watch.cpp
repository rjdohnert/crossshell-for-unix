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

#include <chrono>
#include <ctime>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <memory>

#pragma comment(lib, "user32.lib")

// ============================================================================
// 1. RAII HANDLES & RESOURCE GUARDS
// ============================================================================

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
// 2. ARGUMENT FORMATTER & OPTIONS PARSER
// ============================================================================

class ArgumentFormatter {
public:
    static std::wstring QuoteArgument(const std::wstring& arg) {
        if (arg.empty()) return L"\"\"";
        if (arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) return arg;

        std::wstring quoted = L"\"";
        int backslashes = 0;
        for (wchar_t c : arg) {
            if (c == L'\\') {
                ++backslashes;
            } else if (c == L'\"') {
                quoted.append(backslashes * 2 + 1, L'\\');
                quoted.push_back(L'\"');
                backslashes = 0;
            } else {
                quoted.append(backslashes, L'\\');
                quoted.push_back(c);
                backslashes = 0;
            }
        }
        quoted.append(backslashes * 2, L'\\');
        quoted.push_back(L'\"');
        return quoted;
    }

    static std::wstring JoinCommandArgs(const std::vector<std::wstring>& args) {
        std::wstring result;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) result += L" ";
            result += QuoteArgument(args[i]);
        }
        return result;
    }

    static bool ParseNumber(const std::wstring& text, double& outValue) {
        wchar_t* end = nullptr;
        double parsed = std::wcstod(text.c_str(), &end);
        if (end == text.c_str() || *end != L'\0' || parsed <= 0.0) {
            return false;
        }
        outValue = parsed;
        return true;
    }
};

class WatchOptions {
public:
    double intervalSeconds = 2.0;
    bool showHelp = false;
    bool showVersion = false;
    bool suppressHeader = false;
    std::vector<std::wstring> commandArgs;

    int Parse(int argc, wchar_t* argv[]) {
        bool afterDashDash = false;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";

            if (!afterDashDash && arg == L"--") {
                afterDashDash = true;
                continue;
            }

            if (!afterDashDash && (arg == L"-h" || arg == L"--help")) {
                showHelp = true;
                return 0;
            }

            if (!afterDashDash && arg == L"--version") {
                showVersion = true;
                return 0;
            }

            if (!afterDashDash && (arg == L"-t" || arg == L"--no-title")) {
                suppressHeader = true;
                continue;
            }

            if (!afterDashDash && (arg == L"-n" || arg == L"--interval")) {
                if (i + 1 >= argc) {
                    std::wcerr << L"watch: missing value for " << arg << L"\n";
                    return 2;
                }
                if (!ArgumentFormatter::ParseNumber(argv[++i], intervalSeconds)) {
                    std::wcerr << L"watch: invalid interval: " << argv[i] << L"\n";
                    return 2;
                }
                continue;
            }

            if (!afterDashDash && !arg.empty() && arg[0] == L'-') {
                std::wcerr << L"watch: unknown option -- " << arg << L"\n";
                return 2;
            }

            commandArgs.push_back(arg);
            for (++i; i < argc; ++i) {
                commandArgs.push_back(argv[i]);
            }
            break;
        }

        if (commandArgs.empty() && !showHelp && !showVersion) {
            std::wcerr << L"watch: missing command operand\n";
            return 2;
        }

        return 0;
    }
};

// ============================================================================
// 3. CONSOLE REPORTER & COMMAND RUNNER ENGINE
// ============================================================================

class WatchConsoleReporter {
public:
    static void ClearScreen() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE || hOut == NULL) return;

        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return;

        COORD home = { 0, 0 };
        DWORD cellCount = static_cast<DWORD>(csbi.dwSize.X) * static_cast<DWORD>(csbi.dwSize.Y);
        DWORD written = 0;

        FillConsoleOutputCharacterW(hOut, L' ', cellCount, home, &written);
        FillConsoleOutputAttribute(hOut, csbi.wAttributes, cellCount, home, &written);
        SetConsoleCursorPosition(hOut, home);
    }

    static void PrintHeader(const WatchOptions& options, const std::wstring& displayCommand) {
        if (options.suppressHeader) return;

        std::time_t now = std::time(nullptr);
        wchar_t timeBuf[64] = {};
        std::tm localTime = {};
        localtime_s(&localTime, &now);
        wcsftime(timeBuf, sizeof(timeBuf) / sizeof(timeBuf[0]), L"%Y-%m-%d %H:%M:%S", &localTime);

        std::wcout << L"Every " << options.intervalSeconds << L"s: " << displayCommand
                   << L"    " << timeBuf << L"\n\n";
    }

    static void PrintUsage(const wchar_t* progName) {
        std::wcout
            << L"Usage: " << progName << L" [options] command [args...]\n"
            << L"Run command repeatedly and display output fullscreen.\n\n"
            << L"Options:\n"
            << L"  -n, --interval SEC  Refresh interval in seconds (default: 2.0)\n"
            << L"  -t, --no-title      Do not show the header line\n"
            << L"  -h, --help          Display this help and exit\n"
            << L"      --version       Output version information and exit\n";
    }

    static void PrintVersion() {
        std::wcout << L"watch v1.0.0\n";
    }
};

class CommandRunnerEngine {
public:
    static int RunOnce(const std::wstring& childCmdLine) {
        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION pi = {};
        std::vector<wchar_t> mutableCmd(childCmdLine.begin(), childCmdLine.end());
        mutableCmd.push_back(L'\0');

        BOOL ok = CreateProcessW(
            nullptr,
            mutableCmd.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_UNICODE_ENVIRONMENT,
            nullptr,
            nullptr,
            &si,
            &pi
        );

        if (!ok) {
            std::wcerr << L"watch: failed to execute command (error " << GetLastError() << L")\n";
            return 127;
        }

        ScopedProcessHandle hProcess(pi.hProcess);
        ScopedProcessHandle hThread(pi.hThread);

        WaitForSingleObject(hProcess.Get(), INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(hProcess.Get(), &exitCode);
        return static_cast<int>(exitCode);
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class WatchApplication {
public:
    static BOOL WINAPI CtrlHandler(DWORD type) {
        if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
            ExitProcess(130);
        }
        return FALSE;
    }

    int Run(int argc, wchar_t* argv[]) const {
        SetConsoleCtrlHandler(CtrlHandler, TRUE);

        WatchOptions options;
        int parseStatus = options.Parse(argc, argv);
        if (parseStatus != 0) {
            WatchConsoleReporter::PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"watch");
            return parseStatus;
        }

        if (options.showHelp) {
            WatchConsoleReporter::PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"watch");
            return 0;
        }
        if (options.showVersion) {
            WatchConsoleReporter::PrintVersion();
            return 0;
        }

        const std::wstring userCommand = ArgumentFormatter::JoinCommandArgs(options.commandArgs);
        const std::wstring wrappedCmd = L"cmd.exe /c " + userCommand;

        while (true) {
            WatchConsoleReporter::ClearScreen();
            WatchConsoleReporter::PrintHeader(options, userCommand);
            CommandRunnerEngine::RunOnce(wrappedCmd);

            const auto sleepStep = std::chrono::milliseconds(100);
            const auto totalSleep = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::duration<double>(options.intervalSeconds)
            );
            auto slept = std::chrono::milliseconds(0);
            while (slept < totalSleep) {
                std::this_thread::sleep_for(sleepStep);
                slept += sleepStep;
                if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                    return 0;
                }
            }
        }
    }
};

int wmain(int argc, wchar_t* argv[]) {
    WatchApplication app;
    return app.Run(argc, argv);
}

