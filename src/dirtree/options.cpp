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
#include "path_helper.hpp"
#include <iostream>
#include <cstdlib>

namespace dirtree {

bool TreeOptions::Parse(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--json") { outputFormat = OutputFormat::Json; continue; }
        if (arg == "--csv") { outputFormat = OutputFormat::Csv; continue; }
        if (arg == "--table") { outputFormat = OutputFormat::Table; continue; }
        if (arg == "--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }

        if (arg == "-h" || arg == "--help") {
            showHelp = true;
            return true;
        } else if (arg == "-V" || arg == "--version") {
            showVersion = true;
            return true;
        } else if (arg == "-a" || arg == "--all") {
            showAll = true;
        } else if (arg == "-d" || arg == "--dirs-only") {
            dirsOnly = true;
        } else if (arg == "-s" || arg == "--sizes") {
            showSizes = true;
        } else if (arg == "--ascii") {
            useAscii = true;
        } else if (arg == "-L" || arg == "--level") {
            if (i + 1 < argc) maxLevel = std::atoi(argv[++i]);
        } else if (arg.rfind("--level=", 0) == 0) {
            maxLevel = std::atoi(arg.substr(8).c_str());
        } else if (arg == "-e" || arg == "--extension") {
            if (i + 1 < argc) extensions.push_back(argv[++i]);
        } else if (arg == "-c" || arg == "--color") {
            if (i + 1 < argc) {
                std::string val = argv[++i];
                if (val == "never") {
                    useColor = false;
                    colorModeExplicit = true;
                } else if (val == "always") {
                    useColor = true;
                    colorModeExplicit = true;
                } else if (val == "auto") {
                    useColor = ConsoleEnvironment::ShouldUseColorByDefault();
                    colorModeExplicit = true;
                }
            }
        } else if (arg[0] == '-' && arg.length() > 1) {
            std::cerr << Color::RED << "Error: Unknown option '" << arg << "'" << Color::RESET << "\n";
            std::cerr << "For usage information, run: dirtree --help\n";
            return false;
        } else {
            rootPath = arg;
        }
    }
    return true;
}

void TreeOptions::PrintHelp() const {
    std::cout << R"(dirtree(1)               CrossShell for UNIX Reference Manual                  dirtree(1)

    NAME
        dirtree - Display a tree directory hierarchy

    SYNOPSIS
        dirtree [OPTIONS] [PATH]

    DESCRIPTION
        Displays a directory tree with optional colors, sizes, extension filters,
        structured output, and recursion limits.

    OPTIONS
        -a, --all             Include hidden files and system folders.
        -d, --dirs-only       List directories only.
        -L, --level LEVEL     Set maximum recursion depth.
        -s, --sizes           Display human-readable file sizes.
        -e, --extension EXT   Filter by extension; repeatable.
        --ascii               Use ASCII branch characters.
        -c, --color WHEN      always, never, or auto.
        --json, --csv, --table Select output format.
        --pipe COMMAND        Send output through COMMAND.
        -h, --help            Display this comprehensive reference manual.
        -V, --version         Display version information and exit.

    EXAMPLES
        dirtree
        dirtree -L 2 C:\Projects\App
        dirtree -a -d
        dirtree -e cpp -e hpp -s

    EXIT STATUS
        0          Help, version, or successful traversal.
        1          Invalid options or traversal/output failure.

    CrossShell for UNIX                                                       dirtree(1)
)";
}

void TreeOptions::PrintVersion() const {
    std::cout << Color::BOLD << Color::B_CYAN << "dirtree" << Color::RESET << " version 2.0.0\n";
}

} // namespace dirtree
