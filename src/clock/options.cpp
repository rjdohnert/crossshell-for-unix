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
 */

#include "options.hpp"
#include <iostream>
#include <cstdlib>

void ClockOptions::printUsage() {
    std::cout << R"(clock(1)                CrossShell for UNIX Reference Manual                  clock(1)

    NAME
        clock - report or format the system clock and date

    SYNOPSIS
        clock [OPTIONS] [+FORMAT]

    DESCRIPTION
        Display the current system time and date according to specified formatting
        options or standard time representations.

    OPTIONS
        -R, --rfc-2822
            Output RFC 2822 formatted timestamp.

        -u, --utc, --gmt
            Print Coordinated Universal Time (UTC/GMT).

        --json, --csv, --table
            Output timestamp records in JSON, CSV, or tabular format.

        --pipe COMMAND
            Send output directly through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        clock
            Display current local date and time.

        clock -u
            Display current UTC/GMT date and time.

        clock -R
            Display RFC 2822 formatted timestamp.

        clock "+%Y-%m-%d %H:%M:%S"
            Display custom formatted date and time.

        clock --json
            Display timestamp in JSON format.

    CrossShell for UNIX                                                    clock(1)
)";
}

void ClockOptions::printVersion() {
    std::cout << "clock 1.0.0\n";
}

bool ClockOptions::parse(int argc, char* argv[], ClockOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--json") { opts.outputFormat = 1; continue; }
        if (arg == "--csv") { opts.outputFormat = 2; continue; }
        if (arg == "--table") { opts.outputFormat = 3; continue; }
        if (arg == "--pipe" && i + 1 < argc) { opts.pipeCommand = argv[++i]; continue; }

        if (arg == "--help" || arg == "-h" || arg == "/?") {
            printUsage();
            std::exit(0);
        } else if (arg == "--version" || arg == "-V") {
            printVersion();
            std::exit(0);
        } else if (arg == "-u" || arg == "--utc" || arg == "--gmt") {
            opts.utc = true;
        } else if (arg == "-R" || arg == "--rfc-2822") {
            opts.rfc2822 = true;
        } else if (!arg.empty() && arg[0] == '+') {
            opts.customFormat = arg.substr(1);
        } else {
            std::cerr << "clock: unknown option: " << arg << "\n";
            printUsage();
            return false;
        }
    }
    return true;
}
