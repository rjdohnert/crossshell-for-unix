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

#include "options.hpp"
#include <iostream>

bool ChconOptions::Parse(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        showHelp = true;
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--json") { format = OutputFormat::Json; continue; }
        if (arg == L"--csv") { format = OutputFormat::Csv; continue; }
        if (arg == L"--table") { format = OutputFormat::Table; continue; }
        if (arg == L"--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }
        if (arg == L"--") {
            for (int j = i + 1; j < argc; ++j) {
                targets.push_back(argv[j]);
            }
            break;
        }
        if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
            showHelp = true;
            return true;
        }
        if (arg == L"--version") {
            showVersion = true;
            return true;
        }
        if (arg == L"-R" || arg == L"-r" || arg == L"--recursive") {
            recursive = true;
            continue;
        }
        if (arg == L"-c" || arg == L"--changes") {
            changes = true;
            continue;
        }
        if (arg == L"-v" || arg == L"--verbose") {
            verbose = true;
            continue;
        }
        if (arg == L"-f" || arg == L"--silent" || arg == L"--quiet") {
            quiet = true;
            continue;
        }
        if (contextStr.empty()) {
            contextStr = arg;
        } else {
            targets.push_back(arg);
        }
    }

    if (contextStr.empty() || targets.empty()) {
        return false;
    }

    return true;
}

void ChconOptions::PrintUsage(const wchar_t* /*exe*/) const {
    std::wcout << LR"(chcon(1)                CrossShell for UNIX Reference Manual                 chcon(1)

    NAME
        chcon - change file security context and integrity labels

    SYNOPSIS
        chcon [OPTIONS] CONTEXT FILE...
        chcon [OPTIONS] --reference=RFILE FILE...

    DESCRIPTION
        chcon changes the security context, DACL, or integrity level of each
        FILE according to CONTEXT or from a reference file.

    OPTIONS
        -R, --recursive
            Operate on files and directories recursively.

        --reference=RFILE
            Use RFILE's security context rather than specifying CONTEXT.

        -v, --verbose
            Output a diagnostic for every file processed.

        --json, --csv, --table
            Format result records as JSON, CSV, or table.

        --pipe COMMAND
            Send output directly through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        chcon HighIntegrity C:\Protected\app.exe
            Apply High integrity label to executable.

    CrossShell for UNIX                                                  chcon(1)
)";
}

void ChconOptions::PrintVersion() const {
    std::wcout << L"chcon v1.0.0\n";
}
