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

bool ChrootOptions::Parse(int argc, wchar_t* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--json") { outputFormat = OutputFormat::Json; continue; }
        if (arg == L"--csv") { outputFormat = OutputFormat::Csv; continue; }
        if (arg == L"--table") { outputFormat = OutputFormat::Table; continue; }
        if (arg == L"--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }
        if (arg == L"--") {
            for (int j = i + 1; j < argc; ++j) {
                positional.push_back(argv[j]);
            }
            break;
        } else if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
            showHelp = true;
            return true;
        } else if (arg == L"--version") {
            showVersion = true;
            return true;
        } else if (arg == L"--userspec") {
            if (i + 1 >= argc) {
                std::wcerr << L"chroot: option '--userspec' requires an argument\n";
                return false;
            }
            ++i;
        } else if (arg.rfind(L"--userspec=", 0) == 0) {
            // Accepted for compatibility; no-op on Windows.
        } else {
            positional.push_back(arg);
        }
    }
    return true;
}

void ChrootOptions::PrintHelp() const {
    std::wcout << LR"(chroot(1)               CrossShell for UNIX Reference Manual                chroot(1)

    NAME
        chroot - run command or interactive shell with specified root directory

    SYNOPSIS
        chroot [OPTIONS] NEWROOT [COMMAND [ARG]...]

    DESCRIPTION
        Run COMMAND with root directory set to NEWROOT using isolated Windows
        directory mapping, virtualization, or sub-container environments.

    OPTIONS
        --userspec=USER:GROUP
            Specify user and group to use (numeric IDs or names).

        --groups=G_LIST
            Specify supplementary groups as g1,g2,...,gn.

        --json, --csv, --table
            Format execution diagnostics as JSON, CSV, or table.

        --pipe COMMAND
            Route output into COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Output version information and exit.

    EXAMPLES
        chroot D:\Sandbox cmd.exe
            Run command prompt isolated inside D:\Sandbox.

    CrossShell for UNIX                                                 chroot(1)
)";
}

void ChrootOptions::PrintVersion() const {
    std::wcout << L"chroot 1.0\n";
}
