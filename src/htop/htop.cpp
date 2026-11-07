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

#include "htop_app.hpp"
#include "htop_types.hpp"
#include "htop_version.hpp"

#include <cerrno>
#include <climits>
#include <cwchar>
#include <iostream>
#include <new>
#include <string>

namespace {
bool ParsePositiveInteger(const wchar_t* text, int& value) {
    if (text == nullptr || *text == L'\0') return false;
    errno = 0;
    wchar_t* end = nullptr;
    long parsed = std::wcstol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != L'\0' || parsed <= 0 || parsed > INT_MAX) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

bool ParseProcessId(const wchar_t* text, DWORD& value) {
    if (text == nullptr || *text == L'\0' || *text == L'-') return false;
    errno = 0;
    wchar_t* end = nullptr;
    unsigned long long parsed = std::wcstoull(text, &end, 10);
    if (errno == ERANGE || end == text || *end != L'\0' || parsed > MAXDWORD) return false;
    value = static_cast<DWORD>(parsed);
    return true;
}
}

static int RunCommandLine() {
    int argc = __argc;
    wchar_t** argv = __wargv;

    HtopOptions options;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i] ? argv[i] : L"";
        if (arg == L"--help" || arg == L"-h" || arg == L"/?" || arg == L"-?") {
            std::wcout << LR"(htop(1)            CrossShell for UNIX Reference Manual                 htop(1)

    NAME
        htop - interactive process viewer and system resource monitor

    SYNOPSIS
        htop [OPTIONS]

    DESCRIPTION
        htop is an interactive real-time process monitoring and system management
        tool for Windows NT, displaying per-core CPU meters, memory gauges, process
        trees, and thread telemetry.

    OPTIONS
        -d, --delay DELAY
            Delay between updates in tenths of seconds (default: 15).

        -u, --user USERNAME
            Show only processes owned by the specified user.

        -p, --pid PID...
            Show only processes with the specified process IDs (repeatable).

        -h, --help
            Display this reference manual.

        -V, --version
            Display version information.

    INTERACTIVE KEYS
        F1, h, ?
            Open interactive help screen.

        F2, S
            Open setup and meter configuration.

        F3, /
            Incremental process search.

        F4, \
            Incremental process filter.

        F5, t
            Toggle tree view hierarchy.

        F6, <, >
            Sort by selected column.

        F7, [
            Lower process priority.

        F8, ]
            Raise process priority.

        F9, k
            Open the process action menu.

        F10, q
            Quit the application.

    EXAMPLES
        htop
            Launch interactive process monitor.

        htop -d 5
            Launch with a 0.5-second update interval.

        htop -u SYSTEM
            Show processes running under the SYSTEM account.

        htop -p 1234 5678
            Show only processes 1234 and 5678.

    CrossShell for UNIX                                                    htop(1)
)";
            return 0;
        }
        if (arg == L"--version" || arg == L"-V") {
            std::wcout << AppInfo::NAME << L" " << AppInfo::VERSION << L"\n";
            return 0;
        }

        if (arg == L"-d" || arg == L"--delay") {
            if (i + 1 >= argc || !ParsePositiveInteger(argv[i + 1], options.delayTenths)) {
                std::wcerr << L"htop: delay must be a positive integer number of tenths\n";
                return 2;
            }
            ++i;
        } else if (arg == L"-u" || arg == L"--user") {
            if (i + 1 >= argc || argv[i + 1] == nullptr || argv[i + 1][0] == L'\0' ||
                argv[i + 1][0] == L'-') {
                std::wcerr << L"htop: missing username for " << arg << L"\n";
                return 2;
            }
            options.userFilter = argv[++i];
        } else if (arg == L"-p" || arg == L"--pid") {
            bool parsedAny = false;
            while (i + 1 < argc && argv[i + 1] != nullptr && argv[i + 1][0] != L'-') {
                DWORD pid = 0;
                if (!ParseProcessId(argv[i + 1], pid)) {
                    std::wcerr << L"htop: invalid process ID: " << argv[i + 1] << L"\n";
                    return 2;
                }
                options.processIds.insert(pid);
                parsedAny = true;
                ++i;
            }
            if (!parsedAny) {
                std::wcerr << L"htop: " << arg << L" requires at least one process ID\n";
                return 2;
            }
        } else {
            std::wcerr << L"htop: unknown option: " << arg << L"\n";
            std::wcerr << L"Try 'htop --help' for more information.\n";
            return 2;
        }
    }

    return RunHtopApp(options);
}

int wmain() {
    try {
        return RunCommandLine();
    } catch (const std::bad_alloc&) {
        std::wcerr << L"htop: insufficient memory to continue.\n";
        return 1;
    }
}