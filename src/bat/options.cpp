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
 *
 * CrossShell for UNIX
 */

#include "options.hpp"
#include <cstdlib>
#include <iostream>

void BatOptions::printUsage(const char* prog) {
    (void)prog;
    std::cout << R"(bat(1)                  CrossShell for UNIX Reference Manual                  bat(1)

    NAME
        bat - syntax-highlighted file viewer with Git-style decorations

    SYNOPSIS
        bat [OPTIONS] [FILE...]

    DESCRIPTION
        A cat clone with syntax highlighting and Git-style decorations.
        When FILE is omitted, or FILE is -, read standard input.

    OPTIONS
        -A, --show-all
            Show non-printable characters such as tabs, spaces, and newlines.

        -p, --plain
            Show plain style without line numbers, grid, or header.

        -n, --number
            Show only line numbers without grid or header.

        -l, --language LANGUAGE
            Explicitly set the syntax language.

        --json, --csv, --table
            Emit structured output in JSON, CSV, or tabular format.

        --pipe COMMAND
            Send output through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        bat file.cpp
            Display syntax-highlighted file.

        bat -n script.sh
            Display file with line numbers only.

        bat --language python app.py
            Force Python syntax highlighting.

    CrossShell for UNIX                                                    bat(1)
)";
}

void BatOptions::printVersion() {
    std::cout << "bat 1.0.0\n";
}

bool BatOptions::parse(int argc, char* argv[], BatOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help" || arg == "/?") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "-V" || arg == "--version") {
            printVersion();
            std::exit(0);
        } else if (arg == "-p" || arg == "--plain") {
            opts.plain = true;
            opts.showLineNumbers = false;
            opts.showGrid = false;
            opts.showHeader = false;
        } else if (arg == "-n" || arg == "--number") {
            opts.showLineNumbers = true;
            opts.showGrid = false;
            opts.showHeader = false;
        } else if (arg == "-A" || arg == "--show-all") {
            opts.showAll = true;
        } else if ((arg == "-l" || arg == "--language") && i + 1 < argc) {
            opts.forcedLanguage = argv[++i];
        } else if (arg == "--json") {
            opts.outputFormat = 1;
        } else if (arg == "--csv") {
            opts.outputFormat = 2;
        } else if (arg == "--table") {
            opts.outputFormat = 3;
        } else if (arg == "--pipe" && i + 1 < argc) {
            opts.pipeCommand = argv[++i];
        } else if (arg[0] == '-' && arg != "-") {
            std::cerr << "bat: unknown option " << arg << "\n";
            return false;
        } else {
            opts.files.push_back(arg);
        }
    }

    if (opts.files.empty()) {
        opts.files.push_back("-");
    }

    return true;
}
