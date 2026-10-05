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
#include <cstdlib>
#include <iostream>

void BasenameOptions::printUsage(const char* progName) {
    std::cerr << "usage: " << progName << " NAME [SUFFIX]\n";
    std::cerr << "       " << progName << " OPTION... NAME...\n";
}

void BasenameOptions::printHelp(const char* progName) {
    (void)progName;
    std::cout << R"(basename(1)             CrossShell for UNIX Reference Manual           basename(1)

    NAME
        basename - strip directory and suffix from filenames

    SYNOPSIS
        basename NAME [SUFFIX]
        basename OPTION... NAME...

    DESCRIPTION
        Print NAME with any leading directory components removed. If specified,
        also remove a trailing SUFFIX.

    OPTIONS
        -a, --multiple
            Support multiple arguments and treat each as a NAME.

        -s, --suffix SUFFIX
            Remove a trailing SUFFIX; implies -a.

        -z, --zero
            End each output line with NUL, not newline.

        --json, --csv, --table
            Output basename records in JSON, CSV, or tabular format.

        --pipe COMMAND
            Pipe output directly to COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        basename /usr/bin/sort
            Output "sort".

        basename include/stdio.h .h
            Output "stdio".

        basename -s .h include/stdio.h sys/socket.h
            Output "stdio" and "socket".

    CrossShell for UNIX                                                    basename(1)
)";
}

void BasenameOptions::printVersion() {
    std::cout << "basename 1.0\n";
}

bool BasenameOptions::parse(int argc, char* argv[], BasenameOptions& opts) {
    int i = 1;
    for (; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--json") { opts.outputFormat = 1; continue; }
        if (arg == "--csv") { opts.outputFormat = 2; continue; }
        if (arg == "--table") { opts.outputFormat = 3; continue; }
        if (arg == "--pipe" && i + 1 < argc) { opts.pipeCommand = argv[++i]; continue; }
        if (arg == "--") {
            i++;
            break;
        } else if (arg == "--help" || arg == "-h" || arg == "/?") {
            printHelp(argv[0]);
            std::exit(0);
        } else if (arg == "--version" || arg == "-V") {
            printVersion();
            std::exit(0);
        } else if (arg == "-a" || arg == "--multiple") {
            opts.multiple = true;
        } else if (arg == "-s" || arg == "--suffix") {
            if (i + 1 < argc) {
                opts.suffix = argv[++i];
                opts.multiple = true;
            } else {
                std::cerr << "basename: option requires an argument -- 's'\n";
                printUsage(argv[0]);
                return false;
            }
        } else if (arg.rfind("--suffix=", 0) == 0) {
            opts.suffix = arg.substr(9);
            opts.multiple = true;
        } else if (arg == "-z" || arg == "--zero") {
            opts.zeroTerminated = true;
        } else if (arg[0] == '-' && arg.size() > 1) {
            bool valid = true;
            for (size_t j = 1; j < arg.size(); ++j) {
                if (arg[j] == 'a') {
                    opts.multiple = true;
                } else if (arg[j] == 's') {
                    if (j + 1 < arg.size()) {
                        opts.suffix = arg.substr(j + 1);
                        opts.multiple = true;
                        break;
                    } else if (i + 1 < argc) {
                        opts.suffix = argv[++i];
                        opts.multiple = true;
                        break;
                    } else {
                        std::cerr << "basename: option requires an argument -- 's'\n";
                        printUsage(argv[0]);
                        return false;
                    }
                } else if (arg[j] == 'z') {
                    opts.zeroTerminated = true;
                } else {
                    valid = false;
                    break;
                }
            }
            if (!valid) {
                std::cerr << "basename: invalid option -- '" << arg[1] << "'\n";
                printUsage(argv[0]);
                return false;
            }
        } else {
            break;
        }
    }

    for (; i < argc; ++i) {
        opts.paths.push_back(argv[i]);
    }

    return true;
}
