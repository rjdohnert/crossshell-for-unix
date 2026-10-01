/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, cmd-extended contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
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

/**
 * ============================================================================
 * SINGLE FILE INDEX: spawn.cpp
 * ============================================================================
 * WinSpawn - Object-Oriented VMS-Style Subprocess Launcher for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [RESOURCE WRAPPERS & VMS STATUS] ....... AutoHandle struct and VmsStatusReporter class
 * 2. [OPTIONS & QUALIFIER PARSER] .......... SpawnOptions class (VMS qualifiers /WAIT, /NOWAIT)
 * 3. [SUBPROCESS CONTROLLER] ............... ProcessController class (redirection, launch, wait)
 * 4. [CORE SPAWN ENGINE] ................... SpawnEngine class (subshell resolution & execution)
 * 5. [APPLICATION CONTROLLER] .............. SpawnApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <optional>
#include <algorithm>
#include <cstdio>
#include <memory>

namespace fs = std::filesystem;

// ============================================================================
// 1. RESOURCE WRAPPERS & VMS STATUS
// ============================================================================

struct AutoHandle {
    HANDLE handle{ INVALID_HANDLE_VALUE };

    AutoHandle() = default;
    explicit AutoHandle(HANDLE h) : handle(h) {}
    ~AutoHandle() { if (isValid()) ::CloseHandle(handle); }

    AutoHandle(const AutoHandle&) = delete;
    AutoHandle& operator=(const AutoHandle&) = delete;

    AutoHandle(AutoHandle&& other) noexcept : handle(other.handle) {
        other.handle = INVALID_HANDLE_VALUE;
    }

    AutoHandle& operator=(AutoHandle&& other) noexcept {
        if (this != &other) {
            if (isValid()) ::CloseHandle(handle);
            handle = other.handle;
            other.handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    [[nodiscard]] bool isValid() const noexcept {
        return handle != INVALID_HANDLE_VALUE && handle != nullptr;
    }

    HANDLE get() const noexcept { return handle; }
    HANDLE* addressof() noexcept { return &handle; }
};

class VmsStatusReporter {
private:
    int outputFormat{0};
    std::wstring pipeCommand;

public:
    VmsStatusReporter(int fmt, std::wstring pipeCmd)
        : outputFormat(fmt), pipeCommand(std::move(pipeCmd)) {}

    void printStatus(wchar_t severity, std::wstring_view facility, std::wstring_view ident, std::wstring_view text) const {
        std::wstring line = L"%" + std::wstring(facility) + L"-" + severity + L"-" + std::wstring(ident) + L", " + std::wstring(text);
        if (outputFormat == 1) std::wcout << L"{\"status\":\"" << line << L"\"}\n";
        else if (outputFormat == 2) std::wcout << L"\"status\"\n\"" << line << L"\"\n";
        else if (outputFormat == 3) std::wcout << L"STATUS\n" << line << L"\n";
        else std::wcout << line << L"\n";
    }

    static std::wstring toUpper(std::wstring_view str) {
        std::wstring result(str);
        std::transform(result.begin(), result.end(), result.begin(), ::towupper);
        return result;
    }
};

// ============================================================================
// 2. OPTIONS & QUALIFIER PARSER
// ============================================================================

class SpawnOptions {
public:
    bool showHelp{false};
    bool showVersion{false};
    bool wait{true};                    // Default: /WAIT; /NOWAIT for background
    bool notify{false};                 // /NOTIFY
    std::wstring processName{};          // /PROCESS_NAME="Name"
    std::wstring inputFile{};            // /INPUT=file.dat
    std::wstring outputFile{};           // /OUTPUT=file.log
    std::wstring commandString{};        // The command string or subshell
    int outputFormat{0};
    std::wstring pipeCommand;

    static void printHelp() {
        std::wcout <<
LR"(SPAWN

    Creates a concurrent subprocess and optionally transfers control to it.

Format:

    SPAWN  [command-string]

Command Parameters:

    command-string
        Specifies a command string to be executed in the context of the
        created subprocess. If omitted, an interactive subshell is spawned.

Command Qualifiers:

    /INPUT=file-spec
        Directs standard input from the specified file.

    /OUTPUT=file-spec
        Directs standard output to the specified file.

    /PROCESS_NAME=process-name
        Specifies a descriptive name for the spawned process.

    /WAIT (default)
    /NOWAIT
        Controls whether SPAWN waits for the subprocess to complete before
        returning control to the parent command line.

    /NOTIFY
    /NONOTIFY (default)
        Broadcasts a completion message when a /NOWAIT subprocess finishes.

    /HELP
        Displays this help documentation.

Examples:

    SPAWN
    SPAWN/NOWAIT/NOTIFY/OUTPUT=build.log nmake
    SPAWN/PROCESS_NAME="BackupTask" robocopy C:\Data D:\Backup /MIR
)";
    }

    static void printVersion() {
        std::wcout << L"SPAWN version 1.1.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], SpawnOptions& opts) {
        std::vector<std::wstring> rawArgs;
        for (int i = 1; i < argc; ++i) {
            rawArgs.emplace_back(argv[i]);
        }

        std::vector<std::wstring> cmdTokens;

        for (size_t i = 0; i < rawArgs.size(); ++i) {
            const auto& arg = rawArgs[i];

            if (arg == L"/?" || arg == L"-h" || arg == L"--help" || VmsStatusReporter::toUpper(arg) == L"/HELP") {
                opts.showHelp = true;
                return true;
            }
            if (arg == L"-V" || arg == L"--version" || VmsStatusReporter::toUpper(arg) == L"/VERSION") {
                opts.showVersion = true;
                return true;
            }

            if (!arg.empty() && (arg[0] == L'/' || arg[0] == L'-')) {
                std::wstring uArg = VmsStatusReporter::toUpper(arg);

                if (uArg == L"/WAIT" || uArg == L"--WAIT") {
                    opts.wait = true;
                } else if (uArg == L"/NOWAIT" || uArg == L"--NOWAIT") {
                    opts.wait = false;
                } else if (uArg == L"/NOTIFY" || uArg == L"--NOTIFY") {
                    opts.notify = true;
                } else if (uArg == L"/NONOTIFY" || uArg == L"--NONOTIFY") {
                    opts.notify = false;
                } else if (uArg.rfind(L"/PROCESS_NAME=", 0) == 0 || uArg.rfind(L"--PROCESS_NAME=", 0) == 0) {
                    size_t eq = arg.find(L'=');
                    opts.processName = arg.substr(eq + 1);
                } else if (uArg.rfind(L"/INPUT=", 0) == 0 || uArg.rfind(L"--INPUT=", 0) == 0) {
                    size_t eq = arg.find(L'=');
                    opts.inputFile = arg.substr(eq + 1);
                } else if (uArg.rfind(L"/OUTPUT=", 0) == 0 || uArg.rfind(L"--OUTPUT=", 0) == 0) {
                    size_t eq = arg.find(L'=');
                    opts.outputFile = arg.substr(eq + 1);
                } else {
                    cmdTokens.push_back(arg);
                }
            } else {
                cmdTokens.push_back(arg);
            }
        }

        if (!cmdTokens.empty()) {
            std::wstring joined;
            for (size_t i = 0; i < cmdTokens.size(); ++i) {
                if (i > 0) joined += L" ";
                joined += cmdTokens[i];
            }
            opts.commandString = joined;
        }

        return true;
    }
};

// ============================================================================
// 3. SUBPROCESS CONTROLLER
// ============================================================================

class ProcessController {
public:
    static bool launch(const std::wstring& cmd,
                       const SpawnOptions& opts,
                       const VmsStatusReporter& reporter,
                       DWORD& outExitCode) {
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        AutoHandle hInput;
        AutoHandle hOutput;

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        if (!opts.inputFile.empty()) {
            hInput = AutoHandle(::CreateFileW(
                opts.inputFile.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                &sa,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr));

            if (!hInput.isValid()) {
                reporter.printStatus(L'E', L"SPAWN", L"OPENIN", L"Error opening input file: " + opts.inputFile);
                return false;
            }
            si.dwFlags |= STARTF_USESTDHANDLES;
            si.hStdInput = hInput.get();
        }

        if (!opts.outputFile.empty()) {
            hOutput = AutoHandle(::CreateFileW(
                opts.outputFile.c_str(),
                GENERIC_WRITE,
                FILE_SHARE_READ,
                &sa,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr));

            if (!hOutput.isValid()) {
                reporter.printStatus(L'E', L"SPAWN", L"OPENOUT", L"Error opening output file: " + opts.outputFile);
                return false;
            }
            si.dwFlags |= STARTF_USESTDHANDLES;
            si.hStdOutput = hOutput.get();
            si.hStdError = hOutput.get();
        }

        std::vector<wchar_t> cmdBuffer(cmd.begin(), cmd.end());
        cmdBuffer.push_back(L'\0');

        BOOL created = ::CreateProcessW(
            nullptr,
            cmdBuffer.data(),
            nullptr,
            nullptr,
            TRUE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi);

        if (!created) {
            DWORD err = ::GetLastError();
            reporter.printStatus(L'F', L"SPAWN", L"CREPRC", L"Process creation failed with error " + std::to_wstring(err));
            return false;
        }

        AutoHandle hProc(pi.hProcess);
        AutoHandle hThread(pi.hThread);

        std::wstring name = opts.processName.empty() ? L"SUBPROCESS" : opts.processName;
        reporter.printStatus(L'S', L"SPAWN", L"CREATED", L"Process " + name + L" created (PID: " + std::to_wstring(pi.dwProcessId) + L")");

        if (opts.wait) {
            ::WaitForSingleObject(hProc.get(), INFINITE);
            ::GetExitCodeProcess(hProc.get(), &outExitCode);
            reporter.printStatus(L'S', L"SPAWN", L"RETURNED", L"Control returned to parent process.");
        } else {
            outExitCode = 0;
            if (opts.notify) {
                reporter.printStatus(L'I', L"SPAWN", L"NOWAIT", L"Subprocess executing in background with notification enabled.");
            }
        }

        return true;
    }
};

// ============================================================================
// 4. CORE SPAWN ENGINE
// ============================================================================

class SpawnEngine {
private:
    SpawnOptions options;
    VmsStatusReporter reporter;

public:
    explicit SpawnEngine(SpawnOptions opts)
        : options(opts), reporter(opts.outputFormat, opts.pipeCommand) {}

    int execute() {
        if (options.showHelp) {
            SpawnOptions::printHelp();
            return 0;
        }
        if (options.showVersion) {
            SpawnOptions::printVersion();
            return 0;
        }

        std::wstring fullCommand;
        wchar_t comspec[MAX_PATH];
        DWORD len = ::GetEnvironmentVariableW(L"COMSPEC", comspec, MAX_PATH);
        std::wstring shell = (len > 0 && len < MAX_PATH) ? comspec : L"cmd.exe";

        if (options.commandString.empty()) {
            fullCommand = shell;
        } else {
            fullCommand = shell + L" /c " + options.commandString;
        }

        DWORD exitCode = 0;
        bool ok = ProcessController::launch(fullCommand, options, reporter, exitCode);
        return ok ? static_cast<int>(exitCode) : 1;
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class SpawnApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        SpawnOptions options;
        if (!SpawnOptions::parse(argc, argv, options)) {
            return 1;
        }

        SpawnEngine engine(std::move(options));
        return engine.execute();
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return SpawnApp::run(argc, argv);
}