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

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <sddl.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <atomic>
#include <cwctype>
#include <io.h>
#include <fcntl.h>
#include <conio.h>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "ntdll.lib")

namespace AppInfo {
    constexpr wchar_t NAME[] = L"htop";
    constexpr wchar_t VERSION[] = L"5.9";
}

#include "htop_module_01_ntdll.inc"
#include "htop_module_02_color.inc"
#include "htop_module_03_models.inc"
#include "htop_module_04_telemetry.inc"
#include "htop_module_05_formatters.inc"
#include "htop_module_06_renderer.inc"
#include "htop_module_07_app.inc"

int wmain() {
    int argc = __argc;
    wchar_t** argv = __wargv;

    if (argc > 1) {
        std::wstring arg1 = argv[1] ? argv[1] : L"";
        if (arg1 == L"--help" || arg1 == L"-h" || arg1 == L"/?") {
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
            Delay between updates in tenths of seconds.

        -u, --user USERNAME
            Show only processes owned by the specified user.

        -p, --pid PID...
            Show only processes with the specified process IDs.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

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

        F7, ]
            Increase process priority.

        F8, [
            Decrease process priority.

        F9, k
            Send signal or terminate selected process.

        F10, q
            Quit the application.

    EXAMPLES
        htop
            Launch interactive process monitor.

        htop -d 5
            Launch with a 0.5-second update interval.

        htop -u SYSTEM
            Show processes running under the SYSTEM account.

    CrossShell for UNIX                                                    htop(1)
)";
            return 0;
        }
        if (arg1 == L"--version" || arg1 == L"-V") {
            std::wcout << AppInfo::NAME << L" " << AppInfo::VERSION << L"\n";
            return 0;
        }
    }

    if (!InitializeNtApis()) {
        std::wcerr << L"Error: Failed to bind Native NTDLL APIs.\n";
        return 1;
    }

    HtopApp app;
    app.Run();

    return 0;
}