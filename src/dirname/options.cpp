/*
 * Copyright (c) 2025, R. J. Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "options.hpp"
#include <iostream>
#include <cstdlib>

namespace dirname_util {

void DirnameOptions::printUsage(const char* /*progName*/) {
    std::cout << R"(dirname(1)                 CrossShell for UNIX Reference Manual                 dirname(1)

    NAME
        dirname - strip last component from file name

    SYNOPSIS
        dirname [OPTION] NAME...

    DESCRIPTION
        Output each NAME with its last non-slash component and trailing slashes
        removed; if NAME contains no /'s or \'s, output '.' (meaning the current
        directory).

    OPTIONS
        -z, --zero
            End each output line with NUL, not newline.
        --json
            Output path records as JSON.
        --csv
            Output path records as CSV.
        --table
            Output path records as a table.
        --pipe COMMAND
            Send output through COMMAND.
        -h, --help
            Display this help and exit.
        --version
            Output version information and exit.

    EXAMPLES
        dirname /usr/bin/sort
            Output "/usr/bin".

        dirname stdio.h
            Output ".".

        dirname /etc/nginx/ /var/log/
            Output "/etc" followed by "/var".

    CrossShell for UNIX                                                    dirname(1)
)";
}

void DirnameOptions::printVersion() {
    std::cout << "dirname 1.0\n";
}

bool DirnameOptions::parse(int argc, char* argv[], DirnameOptions& opts) {
    bool stopFlags = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--json") { opts.outputFormat = 1; continue; }
        if (arg == "--csv") { opts.outputFormat = 2; continue; }
        if (arg == "--table") { opts.outputFormat = 3; continue; }
        if (arg == "--pipe" && i + 1 < argc) { opts.pipeCommand = argv[++i]; continue; }

        if (!stopFlags) {
            if (arg == "--") {
                stopFlags = true;
                continue;
            } else if (arg == "-h" || arg == "--help" || arg == "/?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-z" || arg == "--zero" || arg == "-0") {
                opts.zeroTerminated = true;
                continue;
            }
        }
        opts.paths.push_back(arg);
    }

    if (opts.paths.empty()) {
        std::cerr << "dirname: missing operand\n";
        std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
        return false;
    }

    return true;
}

} // namespace dirname_util
