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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. RAII HANDLES & PIPE ENCODING UTILITIES
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

class NativePipeConfigurator {
public:
    static std::string ToUtf8(const std::wstring& text) {
        if (text.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string result(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    static void PipeOutput(const std::wstring& pipeCommand, const std::wstring& text) {
        if (pipeCommand.empty()) {
            std::wcout << text;
            return;
        }
        FILE* pipe = _wpopen(pipeCommand.c_str(), L"w");
        if (!pipe) {
            ExitProcess(1);
        }
        std::string narrow = ToUtf8(text);
        fwrite(narrow.data(), 1, narrow.size(), pipe);
        _pclose(pipe);
    }
};

// ============================================================================
// 2. OPTIONS PARSER
// ============================================================================

enum class OutputFormat {
    None = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

class ExecOptions {
public:
    bool cleanEnv = false;
    bool loginShell = false;
    std::wstring altArg0;
    OutputFormat format = OutputFormat::None;
    std::wstring pipeCommand;
    bool showHelp = false;
    bool showVersion = false;
    int commandIndex = -1;

    bool Parse(int argc, wchar_t* argv[]) {
        int argIdx = 1;
        for (; argIdx < argc; ++argIdx) {
            std::wstring arg = argv[argIdx];
            if (arg == L"--json") { format = OutputFormat::Json; continue; }
            if (arg == L"--csv") { format = OutputFormat::Csv; continue; }
            if (arg == L"--table") { format = OutputFormat::Table; continue; }
            if (arg == L"--pipe" && argIdx + 1 < argc) { pipeCommand = argv[++argIdx]; continue; }
            
            if (arg == L"--help") {
                showHelp = true;
                return true;
            } else if (arg == L"--version") {
                showVersion = true;
                return true;
            } else if (arg == L"--") {
                argIdx++;
                break;
            } else if (arg == L"-c" || arg == L"--clear") {
                cleanEnv = true;
            } else if (arg == L"-l" || arg == L"--login") {
                loginShell = true;
            } else if (arg == L"-a" || arg == L"--name") {
                if (argIdx + 1 < argc) {
                    altArg0 = argv[++argIdx];
                } else {
                    std::wcerr << L"exec: option requires an argument -- " << (arg == L"-a" ? L"a" : L"name") << L"\n";
                    return false;
                }
            } else if (arg.rfind(L"--name=", 0) == 0) {
                altArg0 = arg.substr(7);
            } else if (!arg.empty() && arg[0] == L'-' && arg.length() > 1) {
                bool valid = true;
                size_t i = 1;
                while (i < arg.length()) {
                    if (arg[i] == L'c') {
                        cleanEnv = true;
                        ++i;
                    } else if (arg[i] == L'l') {
                        loginShell = true;
                        ++i;
                    } else if (arg[i] == L'a') {
                        if (i + 1 < arg.length()) {
                            altArg0 = arg.substr(i + 1);
                            break;
                        }
                        if (argIdx + 1 < argc) {
                            altArg0 = argv[++argIdx];
                        } else {
                            std::wcerr << L"exec: option requires an argument -- a\n";
                            return false;
                        }
                        break;
                    } else {
                        valid = false;
                        break;
                    }
                }
                if (!valid) {
                    std::wcerr << L"exec: invalid option -- " << arg << L"\n";
                    PrintUsage();
                    return false;
                }
            } else {
                break;
            }
        }

        commandIndex = argIdx;
        return true;
    }

    void PrintUsage() const {
        std::wcout << LR"(exec(1)                 CrossShell for UNIX Reference Manual                  exec(1)

    NAME
        exec - replace current process image or execute clean process

    SYNOPSIS
        exec [OPTIONS] COMMAND [ARGUMENTS...]

    DESCRIPTION
        exec executes COMMAND in place of the current process or with a clean,
        isolated environment.

    OPTIONS
        -c, --clear
            Execute with an empty environment.

        -l, --login
            Pass 0th argument with leading hyphen for login shell simulation.

        --json, --csv, --table
            Format launch diagnostics in structured format.

        --pipe COMMAND
            Stream output to COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Output version information and exit.

    EXAMPLES
        exec -c cmd.exe
            Replace shell with a clean environment cmd.exe session.

    CrossShell for UNIX                                                   exec(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"exec v1.0.0\n";
    }
};

// ============================================================================
// 3. PROCESS LAUNCHER ENGINE & REPORTER
// ============================================================================

class ProcessLauncher {
public:
    static std::wstring QuoteArgument(const std::wstring& arg) {
        if (arg.empty()) {
            return L"\"\"";
        }
        if (arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
            return arg;
        }

        std::wstring quoted = L"\"";
        int backslashes = 0;
        for (wchar_t c : arg) {
            if (c == L'\\') {
                backslashes++;
            } else if (c == L'"') {
                quoted.append(backslashes * 2 + 1, L'\\');
                quoted.push_back(L'"');
                backslashes = 0;
            } else {
                quoted.append(backslashes, L'\\');
                quoted.push_back(c);
                backslashes = 0;
            }
        }
        quoted.append(backslashes * 2, L'\\');
        quoted.push_back(L'"');
        return quoted;
    }

    static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
        switch (dwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
            return TRUE;
        default:
            return FALSE;
        }
    }

    static int ExecuteCommand(int argc, wchar_t* argv[], const ExecOptions& options, DWORD& outExitCode) {
        if (options.commandIndex >= argc) {
            return 0;
        }

        std::wstring command = argv[options.commandIndex];
        std::wstring arg0 = command;
        if (!options.altArg0.empty()) {
            arg0 = options.altArg0;
        }
        if (options.loginShell) {
            arg0 = L"-" + arg0;
        }

        std::wstring cmdline = QuoteArgument(arg0);
        for (int i = options.commandIndex + 1; i < argc; ++i) {
            cmdline += L" " + QuoteArgument(argv[i]);
        }

        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION pi = {};
        SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

        wchar_t empty_env[] = L"\0\0";
        LPVOID pEnv = options.cleanEnv ? static_cast<LPVOID>(empty_env) : NULL;

        BOOL success = CreateProcessW(
            NULL,
            &cmdline[0],
            NULL,
            NULL,
            TRUE,
            CREATE_UNICODE_ENVIRONMENT,
            pEnv,
            NULL,
            &si,
            &pi
        );

        if (!success) {
            DWORD err = GetLastError();
            if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
                std::wcerr << L"exec: " << command << L": command not found\n";
                return 127;
            } else {
                std::wcerr << L"exec: " << command << L": failed to execute (Error " << err << L")\n";
                return 126;
            }
        }

        ScopedProcessHandle hProcess(pi.hProcess);
        ScopedProcessHandle hThread(pi.hThread);

        WaitForSingleObject(hProcess.Get(), INFINITE);
        GetExitCodeProcess(hProcess.Get(), &outExitCode);

        return 0;
    }
};

class ExecReporter {
public:
    static void ReportStatus(OutputFormat format, const std::wstring& pipeCommand, DWORD exitCode) {
        if (format == OutputFormat::None && pipeCommand.empty()) {
            return;
        }

        std::wstring text;
        if (format == OutputFormat::Json) {
            text = L"{\"status\":\"completed\",\"exit_code\":" + std::to_wstring(exitCode) + L"}\n";
        } else if (format == OutputFormat::Csv) {
            text = L"status,exit_code\ncompleted," + std::to_wstring(exitCode) + L"\n";
        } else if (format == OutputFormat::Table) {
            text = L"STATUS\tEXIT_CODE\ncompleted\t" + std::to_wstring(exitCode) + L"\n";
        } else {
            text = L"completed: " + std::to_wstring(exitCode) + L"\n";
        }

        NativePipeConfigurator::PipeOutput(pipeCommand, text);
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class ExecApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        ExecOptions options;
        if (!options.Parse(argc, argv)) {
            return 1;
        }

        if (options.showHelp) {
            options.PrintUsage();
            return 0;
        }

        if (options.showVersion) {
            options.PrintVersion();
            return 0;
        }

        if (options.commandIndex >= argc) {
            return 0;
        }

        DWORD exitCode = 0;
        int launchResult = ProcessLauncher::ExecuteCommand(argc, argv, options, exitCode);
        if (launchResult != 0) {
            return launchResult;
        }

        ExecReporter::ReportStatus(options.format, options.pipeCommand, exitCode);
        ExitProcess(exitCode);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    ExecApplication app;
    return app.Run(argc, argv);
}
