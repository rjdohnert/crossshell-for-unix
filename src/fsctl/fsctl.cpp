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

#include <windows.h>
#include <aclapi.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cwctype>
#include <winnetwk.h>
#include <wininet.h>
#include <shellapi.h>
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <memory>

#pragma comment(lib, "mpr.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "Advapi32.lib")

#include "fsctl_module_01_models.inc"
#include "fsctl_module_02_helpers.inc"
#include "fsctl_module_03_remote.inc"
#include "fsctl_module_04_filesystem.inc"
#include "fsctl_module_05_app.inc"

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--help" || arg == "-h" || arg == "/?") {
            std::cout << R"(fsctl(1)            CrossShell for UNIX Reference Manual                 fsctl(1)

    NAME
        fsctl - interactive filesystem controller, remote browser, and explorer

    SYNOPSIS
        fsctl [OPTIONS]

    DESCRIPTION
        fsctl is an interactive filesystem shell and storage manager providing
        local disk inspection, network share navigation, FTP/SSH remote
        browsing, and safe destructive action confirmations.

    OPTIONS
        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    INTERACTIVE COMMANDS
        drive
            List available drives with storage info.

        shares
            List available network shares.

        ls, dir [-h] [-t]
            List files and directories.

        tree [path]
            Display directory structure in tree format.

        cd <directory>
            Change current directory or remote URL.

        pwd
            Print current local directory or remote URI.

        mkdir <name>
            Create a new folder hierarchy.

        rmdir <name>
            Remove an empty folder.

        trash [-f] <path>
            Move file or folder to Recycle Bin.

        rm [-r] [-f] <path>
            Remove a file or recursively remove a directory.

        touch [-c] <path>
            Create or update file timestamp.

        cat, type <file>
            Display text file content.

        stat <path>
            Display metadata of a path.

        find <pattern>
            Recursively search for matching filenames.

        diff <f1> <f2>
            Compare two local files line by line.

        history [clear]
            Show command history.

        confirm [on|off]
            Toggle confirmation for destructive operations.

        exit, quit
            Terminate the interactive shell.

    EXAMPLES
        fsctl
            Launch the interactive filesystem controller shell.

    CrossShell for UNIX                                                    fsctl(1)
)";
            return 0;
        }
        if (arg == "--version" || arg == "-V") {
            std::cout << "fsctl 1.0.0\n";
            return 0;
        }
    }
    FsctlApplication app;
    return app.Run();
}
