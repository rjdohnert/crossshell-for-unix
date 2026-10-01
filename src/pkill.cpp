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
#include <tlhelp32.h>

#include <iostream>
#include <string>
#include <vector>
#include <cwctype>
#include <memory>

// ============================================================================
// 1. DATA MODELS & RAII HANDLES
// ============================================================================

struct ProcessInfo {
    DWORD pid = 0;
    std::wstring name;
};

class ScopedSnapshotHandle {
public:
    explicit ScopedSnapshotHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}

    ~ScopedSnapshotHandle() {
        Close();
    }

    ScopedSnapshotHandle(const ScopedSnapshotHandle&) = delete;
    ScopedSnapshotHandle& operator=(const ScopedSnapshotHandle&) = delete;

    ScopedSnapshotHandle(ScopedSnapshotHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }

    ScopedSnapshotHandle& operator=(ScopedSnapshotHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != INVALID_HANDLE_VALUE && m_handle != NULL; }

    void Close() {
        if (m_handle != INVALID_HANDLE_VALUE && m_handle != NULL) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
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
// 2. OPTIONS PARSER
// ============================================================================

class PkillOptions {
public:
    bool ignoreCase = false;
    bool exact = false;
    bool echo = false;
    bool showHelp = false;
    bool showVersion = false;
    std::wstring pattern;

    int Parse(int argc, wchar_t* argv[]) {
        bool afterDoubleDash = false;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";

            if (!afterDoubleDash && arg == L"--") {
                afterDoubleDash = true;
                continue;
            }

            if (!afterDoubleDash && (arg == L"-h" || arg == L"--help")) {
                showHelp = true;
                return 0;
            }

            if (!afterDoubleDash && arg == L"--version") {
                showVersion = true;
                return 0;
            }

            if (!afterDoubleDash && (arg == L"-i" || arg == L"--ignore-case")) {
                ignoreCase = true;
                continue;
            }

            if (!afterDoubleDash && (arg == L"-x" || arg == L"--exact")) {
                exact = true;
                continue;
            }

            if (!afterDoubleDash && (arg == L"-e" || arg == L"--echo")) {
                echo = true;
                continue;
            }

            if (!afterDoubleDash && !arg.empty() && arg[0] == L'-') {
                std::wcerr << L"pkill: unknown option -- " << arg << L"\n";
                return 2;
            }

            if (!pattern.empty()) {
                std::wcerr << L"pkill: only one pattern is supported\n";
                return 2;
            }
            pattern = arg;
        }

        if (pattern.empty()) {
            std::wcerr << L"pkill: missing pattern\n";
            PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"pkill");
            return 2;
        }

        return -1;
    }

    void PrintUsage(const wchar_t* programName) const {
        std::wcout << LR"(pkill(1)                CrossShell for UNIX Reference Manual                 pkill(1)

    NAME
        pkill - look up or signal processes based on name and attributes

    SYNOPSIS
        pkill [OPTIONS] PATTERN

    DESCRIPTION
        pkill looks through the currently running processes and terminates or
        signals processes matching the specified PATTERN regular expression.

    OPTIONS
        -f, --full
            Match pattern against full command line instead of executable name.

        -i, --ignore-case
            Match case-insensitively.

        -e, --echo
            Display what process is being signaled/killed.

        -signal, --signal SIGNAL
            Send specified signal (e.g., 9, 15, KILL, TERM).

        --json, --csv, --table
            Format termination results as JSON, CSV, or table.

        --pipe COMMAND
            Send results through COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Output version information and exit.

    EXAMPLES
        pkill -i notepad
            Kill all processes whose name contains notepad.

        pkill -f -e "node server.js"
            Kill node server matching full command line.

    CrossShell for UNIX                                                  pkill(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"pkill v1.0.0\n";
    }
};

// ============================================================================
// 3. PROCESS MATCHER ENGINE
// ============================================================================

class ProcessMatcherEngine {
public:
    static std::wstring ToLowerCopy(const std::wstring& input) {
        std::wstring result;
        result.reserve(input.size());
        for (wchar_t ch : input) {
            result.push_back(static_cast<wchar_t>(std::towlower(ch)));
        }
        return result;
    }

    static bool CollectProcesses(std::vector<ProcessInfo>& out) {
        out.clear();

        ScopedSnapshotHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
        if (!snap.IsValid()) {
            return false;
        }

        PROCESSENTRY32W pe = {};
        pe.dwSize = sizeof(pe);
        if (!Process32FirstW(snap.Get(), &pe)) {
            return false;
        }

        do {
            ProcessInfo p;
            p.pid = pe.th32ProcessID;
            p.name = pe.szExeFile;
            out.push_back(p);
        } while (Process32NextW(snap.Get(), &pe));

        return true;
    }

    static bool IsMatch(const std::wstring& name, const PkillOptions& options) {
        std::wstring lhs = name;
        std::wstring rhs = options.pattern;
        if (options.ignoreCase) {
            lhs = ToLowerCopy(lhs);
            rhs = ToLowerCopy(rhs);
        }

        if (options.exact) {
            return lhs == rhs;
        }
        return lhs.find(rhs) != std::wstring::npos;
    }

    static int TerminateMatchingProcesses(const PkillOptions& options) {
        std::vector<ProcessInfo> processes;
        if (!CollectProcesses(processes)) {
            std::wcerr << L"pkill: failed to enumerate processes\n";
            return 2;
        }

        const DWORD selfPid = GetCurrentProcessId();
        int matched = 0;
        int killed = 0;

        for (const auto& p : processes) {
            if (!IsMatch(p.name, options)) {
                continue;
            }
            if (p.pid == selfPid || p.pid == 0) {
                continue;
            }

            ++matched;
            ScopedProcessHandle hProcess(OpenProcess(PROCESS_TERMINATE, FALSE, p.pid));
            if (!hProcess.IsValid()) {
                std::wcerr << L"pkill: cannot open process " << p.pid << L" (" << p.name << L")\n";
                continue;
            }

            if (TerminateProcess(hProcess.Get(), 1)) {
                ++killed;
                if (options.echo) {
                    std::wcout << L"killed " << p.pid << L" " << p.name << L"\n";
                }
            } else {
                std::wcerr << L"pkill: failed to terminate " << p.pid << L" (" << p.name << L")\n";
            }
        }

        if (matched == 0 || killed == 0) {
            return 1;
        }
        return 0;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class PkillApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        PkillOptions options;
        int parseResult = options.Parse(argc, argv);
        if (parseResult >= 0) {
            if (options.showHelp) {
                options.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"pkill");
                return 0;
            }
            if (options.showVersion) {
                options.PrintVersion();
                return 0;
            }
            return parseResult;
        }

        return ProcessMatcherEngine::TerminateMatchingProcesses(options);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    PkillApplication app;
    return app.Run(argc, argv);
}

