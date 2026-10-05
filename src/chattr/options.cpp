/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 *
 * CrossShell for UNIX
 */

#include "options.hpp"
#include <iostream>

bool ChattrOptions::Parse(int argc, wchar_t* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i] ? argv[i] : L"";
        if (arg == L"--json") { format = OutputFormat::Json; continue; }
        if (arg == L"--csv") { format = OutputFormat::Csv; continue; }
        if (arg == L"--table") { format = OutputFormat::Table; continue; }
        if (arg == L"--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }
        if (arg == L"--") {
            for (++i; i < argc; ++i) {
                paths.push_back(argv[i] ? argv[i] : L"");
            }
            break;
        }
        if (arg == L"-h" || arg == L"--help") {
            showHelp = true;
            return true;
        }
        if (arg == L"-V" || arg == L"--version") {
            showVersion = true;
            return true;
        }
        if (arg == L"-R" || arg == L"--recursive") {
            recursive = true;
            continue;
        }
        if (arg == L"-p") {
            statusOnly = true;
            continue;
        }
        if (arg == L"-f") {
            continue;
        }
        if (!arg.empty() && (arg[0] == L'+' || arg[0] == L'-')) {
            specs.push_back(arg);
            continue;
        }
        paths.push_back(arg);
    }

    if (paths.empty() && !showHelp && !showVersion) {
        return false;
    }

    return true;
}

void ChattrOptions::PrintUsage(const wchar_t* progName) const {
    (void)progName;
    std::wcout << LR"(chattr(1)               CrossShell for UNIX Reference Manual                chattr(1)

    NAME
        chattr - change file attributes on Windows NTFS/ReFS filesystems

    SYNOPSIS
        chattr [OPTIONS] [+/-ATTR...] FILE...

    DESCRIPTION
        chattr changes the file attributes on a Windows filesystem using Unix-style
        operator syntax (+, -, =). Supports Archive (a), Hidden (h), System (s),
        and Read-Only/Immutable (i).

    OPTIONS
        -R, --recursive
            Recursively change attributes of directories and their contents.

        -p
            Print attributes without changing them.

        --json, --csv, --table
            Output structured status as JSON, CSV, or table.

        --pipe COMMAND
            Send output through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        chattr +r file.txt
            Make file.txt read-only.

        chattr -R +h .git
            Recursively mark directory hidden.

    CrossShell for UNIX                                                 chattr(1)
)";
}

void ChattrOptions::PrintVersion() const {
    std::wcout << L"chattr 1.0.0\n";
}
