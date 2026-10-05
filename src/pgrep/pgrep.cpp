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
#include <algorithm>
#include <cwctype>

// ============================================================================
// 1. DATA MODEL & PROCESS SNAPSHOT COLLECTOR
// ============================================================================

struct ProcessRecord {
    DWORD pid = 0;
    std::wstring name;
};

class ProcessSnapshot {
public:
    static bool Collect(std::vector<ProcessRecord>& out) {
        out.clear();

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) {
            return false;
        }

        PROCESSENTRY32W pe = {};
        pe.dwSize = sizeof(pe);
        if (!Process32FirstW(snap, &pe)) {
            CloseHandle(snap);
            return false;
        }

        do {
            ProcessRecord p;
            p.pid = pe.th32ProcessID;
            p.name = pe.szExeFile;
            out.push_back(p);
        } while (Process32NextW(snap, &pe));

        CloseHandle(snap);
        return true;
    }
};

// ============================================================================
// 2. PROCESS MATCHER & OPTION PARSER
// ============================================================================

struct PgrepOptions {
    bool ignoreCase = false;
    bool exact = false;
    bool listName = false;
    bool countOnly = false;
    bool showHelp = false;
    bool showVersion = false;
    std::wstring pattern;
};

class ProcessMatcher {
public:
    static std::wstring ToLowerCopy(const std::wstring& input) {
        std::wstring result;
        result.reserve(input.size());
        for (wchar_t ch : input) {
            result.push_back(static_cast<wchar_t>(std::towlower(ch)));
        }
        return result;
    }

    static bool IsMatch(const std::wstring& name, const PgrepOptions& options) {
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
};

class OptionParser {
public:
    static void PrintUsage(const wchar_t* programName) {
           std::wcout << LR"HELP(pgrep(1)                 CrossShell for UNIX Reference Manual                   pgrep(1)

    NAME
        pgrep - find running processes by executable name

    SYNOPSIS
        pgrep [OPTIONS] PATTERN

    DESCRIPTION
        Matches running processes by executable name using substring matching by
        default. Process enumeration uses the Windows ToolHelp API.

    OPTIONS
        -i, --ignore-case   Ignore case distinctions.
        -x, --exact         Require an exact executable-name match.
        -l, --list-name     Print PID and process name.
        -c, --count         Print only the match count.
        -h, --help          Display this comprehensive reference manual and exit.
        --version           Display version information and exit.
        --                  End options; the remaining argument is PATTERN.

    EXAMPLES
        pgrep chrome
            Print PIDs for processes whose names contain chrome.
        pgrep -l -i notepad
            Print matching PIDs and names case-insensitively.
        pgrep -c --exact powershell.exe
            Print the count of exact matches.

    EXIT STATUS
        0          Help, version, or at least one matching process.
        1          No matching processes.
        2          Invalid arguments or process snapshot failure.

    CrossShell for UNIX                                                       pgrep(1)
    )HELP";
           return;

        std::wcout
            << L"Usage: " << programName << L" [options] pattern\n"
            << L"Match running processes by executable name.\n\n"
            << L"Options:\n"
            << L"  -i, --ignore-case   Ignore case distinctions\n"
            << L"  -x, --exact         Require exact name match\n"
            << L"  -l, --list-name     List PID and process name\n"
            << L"  -c, --count         Print match count only\n"
            << L"  -h, --help          Display this help and exit\n"
            << L"      --version       Output version information and exit\n";
    }

    static void PrintVersion() {
        std::wcout << L"pgrep v1.0.0\n";
    }

    bool Parse(int argc, wchar_t* argv[], PgrepOptions& options) const {
        bool afterDoubleDash = false;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";

            if (!afterDoubleDash && arg == L"--") {
                afterDoubleDash = true;
                continue;
            }

            if (!afterDoubleDash && (arg == L"-h" || arg == L"--help")) {
                options.showHelp = true;
                return true;
            }

            if (!afterDoubleDash && arg == L"--version") {
                options.showVersion = true;
                return true;
            }

            if (!afterDoubleDash && (arg == L"-i" || arg == L"--ignore-case")) {
                options.ignoreCase = true;
                continue;
            }

            if (!afterDoubleDash && (arg == L"-x" || arg == L"--exact")) {
                options.exact = true;
                continue;
            }

            if (!afterDoubleDash && (arg == L"-l" || arg == L"--list-name")) {
                options.listName = true;
                continue;
            }

            if (!afterDoubleDash && (arg == L"-c" || arg == L"--count")) {
                options.countOnly = true;
                continue;
            }

            if (!afterDoubleDash && !arg.empty() && arg[0] == L'-') {
                std::wcerr << L"pgrep: unknown option -- " << arg << L"\n";
                return false;
            }

            if (!options.pattern.empty()) {
                std::wcerr << L"pgrep: only one pattern is supported\n";
                return false;
            }
            options.pattern = arg;
        }

        if (options.pattern.empty()) {
            std::wcerr << L"pgrep: missing pattern\n";
            return false;
        }

        return true;
    }
};

// ============================================================================
// 3. APPLICATION CONTROLLER
// ============================================================================

class PgrepApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) {
        PgrepOptions options;
        if (!m_parser.Parse(argc, argv, options)) {
            OptionParser::PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"pgrep");
            return 2;
        }

        if (options.showHelp) {
            OptionParser::PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"pgrep");
            return 0;
        }

        if (options.showVersion) {
            OptionParser::PrintVersion();
            return 0;
        }

        std::vector<ProcessRecord> processes;
        if (!ProcessSnapshot::Collect(processes)) {
            std::wcerr << L"pgrep: failed to enumerate processes\n";
            return 2;
        }

        std::vector<ProcessRecord> matches;
        for (const auto& p : processes) {
            if (ProcessMatcher::IsMatch(p.name, options)) {
                matches.push_back(p);
            }
        }

        if (options.countOnly) {
            std::wcout << matches.size() << L"\n";
            return matches.empty() ? 1 : 0;
        }

        for (const auto& p : matches) {
            if (options.listName) {
                std::wcout << p.pid << L" " << p.name << L"\n";
            } else {
                std::wcout << p.pid << L"\n";
            }
        }

        return matches.empty() ? 1 : 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    PgrepApplication app;
    return app.Run(argc, argv);
}

