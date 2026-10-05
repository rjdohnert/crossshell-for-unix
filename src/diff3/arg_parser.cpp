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

#include "arg_parser.hpp"
#include <iostream>
#include <vector>

namespace Diff3Util {

void HelpSystem::print_version() {
    std::cout << "diff3 version 3.6.2\n"
              << "Copyright (C) 2026, Roberto J Dohnert.\n";
}

void HelpSystem::print_help() {
    std::cout << R"(diff3(1)                   CrossShell for UNIX Reference Manual                   diff3(1)

    NAME
        diff3 - compare three files line by line

    SYNOPSIS
        diff3 [OPTION]... MYFILE OLDFILE YOURFILE

    DESCRIPTION
        Compare three files line by line.

        An operand of '-' refers to standard input (at most one file may be '-').

    OPTIONS
        -m, --merge
            Output merged file with conflict brackets.
        -A, --show-all
            Output merged file, bracketing all conflicts with base.
        -e, --ed
            Output an ed script incorporating changes from OLDFILE to YOURFILE.
        -E, --show-overlap
            Like -e, but bracket overlapping changes.
        -3, --easy-only
            Like -e, but incorporate only nonoverlapping changes.
        -x, --overlap-only
            Like -e, but incorporate only overlapping changes.
        -T, --initial-tab
            Output a tab before normal diff3 output lines.
        -L, --label=LABEL
            Use LABEL instead of file name (can be repeated up to 3 times).
        -h, --help
            Display this help and exit.
        -v, --version
            Output version information and exit.

    EXAMPLES
        diff3 mine.txt base.txt yours.txt
            Default 3-way difference report.

        diff3 -m mine.txt base.txt yours.txt
            3-way merge to standard output with conflict markers.

        diff3 -m -L "Local" -L "Ancestor" -L "Remote" local.cpp base.cpp remote.cpp
            Merge with custom labels.

    CrossShell for UNIX                                                    diff3(1)
)";
}

bool ArgParser::parse(int argc, char* argv[], Options& opts) {
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            opts.show_help = true;
            return true;
        } else if (arg == "-v" || arg == "--version") {
            opts.show_version = true;
            return true;
        } else if (arg == "-m" || arg == "--merge") {
            opts.mode = OutputMode::Merge;
        } else if (arg == "-A" || arg == "--show-all") {
            opts.mode = OutputMode::Merge;
            opts.show_base_in_conflict = true;
        } else if (arg == "-e" || arg == "--ed") {
            opts.mode = OutputMode::EdAll;
        } else if (arg == "-E" || arg == "--show-overlap") {
            opts.mode = OutputMode::EdShowOverlap;
        } else if (arg == "-x" || arg == "--overlap-only") {
            opts.mode = OutputMode::EdOverlap;
        } else if (arg == "-3" || arg == "--easy-only") {
            opts.mode = OutputMode::EdNonOverlap;
        } else if (arg == "-T" || arg == "--initial-tab") {
            opts.initial_tab = true;
        } else if (arg == "-L" || arg == "--label") {
            if (i + 1 < argc) {
                opts.labels.push_back(argv[++i]);
            } else {
                std::cerr << "diff3: option requires an argument -- 'L'\n";
                return false;
            }
        } else if (arg.rfind("-L", 0) == 0 && arg.length() > 2) {
            opts.labels.push_back(arg.substr(2));
        } else if (arg.rfind("--label=", 0) == 0) {
            opts.labels.push_back(arg.substr(8));
        } else if (arg.length() > 1 && arg[0] == '-' && arg != "-") {
            // Clustered short flags
            for (size_t j = 1; j < arg.length(); ++j) {
                switch (arg[j]) {
                    case 'm': opts.mode = OutputMode::Merge; break;
                    case 'A': opts.mode = OutputMode::Merge; opts.show_base_in_conflict = true; break;
                    case 'e': opts.mode = OutputMode::EdAll; break;
                    case 'E': opts.mode = OutputMode::EdShowOverlap; break;
                    case 'x': opts.mode = OutputMode::EdOverlap; break;
                    case '3': opts.mode = OutputMode::EdNonOverlap; break;
                    case 'T': opts.initial_tab = true; break;
                    default:
                        std::cerr << "diff3: invalid option -- '" << arg[j] << "'\n";
                        return false;
                }
            }
        } else {
            positional.push_back(arg);
        }
    }

    if (opts.show_help || opts.show_version) return true;

    if (positional.size() < 3) {
        std::cerr << "diff3: missing operand\nTry 'diff3 --help' for more information.\n";
        return false;
    }
    if (positional.size() > 3) {
        std::cerr << "diff3: extra operand '" << positional[3] << "'\nTry 'diff3 --help' for more information.\n";
        return false;
    }

    opts.file_mine = positional[0];
    opts.file_old = positional[1];
    opts.file_yours = positional[2];

    // Verify stdin is referenced at most once
    int stdin_count = (opts.file_mine == "-" ? 1 : 0) +
                      (opts.file_old == "-" ? 1 : 0) +
                      (opts.file_yours == "-" ? 1 : 0);
    if (stdin_count > 1) {
        std::cerr << "diff3: standard input cannot be used for multiple operands\n";
        return false;
    }

    return true;
}

} // namespace Diff3Util
