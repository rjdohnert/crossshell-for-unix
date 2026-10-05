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
#include <cctype>

namespace DiffUtil {

void HelpSystem::print_version() {
    std::cout << "diff version 3.1.0\n"
              << "Copyright (C) 2026, Roberto J Dohnert.\n";
}

void HelpSystem::print_help() {
    std::cout << R"(diff(1)                 CrossShell for UNIX Reference Manual                  diff(1)

    NAME
        diff - compare files line by line

    SYNOPSIS
        diff [OPTIONS] FILE1 FILE2

    DESCRIPTION
        Compare files line by line and report differences. When FILE1 or FILE2
        is '-', read standard input.

    OPTIONS
        -u, -U NUM, --unified[=NUM]
            Output NUM (default: 3) lines of unified context.

        -q, --brief
            Report only when files differ.

        -i, --ignore-case
            Ignore case differences in file contents.

        -w, --ignore-all-space
            Ignore all white space.

        -b, --ignore-space-change
            Ignore changes in the amount of white space.

        -B, --ignore-blank-lines
            Ignore changes where lines are all blank.

        -s, --report-identical-files
            Report when two files are identical.

        --strip-trailing-cr
            Strip trailing carriage return on input.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version and license information.

    EXAMPLES
        diff file1.txt file2.txt
            Compare files in standard normal diff format.

        diff -u -U 5 file1.txt file2.txt
            Produce unified diff with 5 context lines.

        diff -iwb file1.txt file2.txt
            Ignore case and whitespace changes while comparing.

    CrossShell for UNIX                                                    diff(1)
)";
}

bool ArgParser::parse(int argc, char* argv[], Options& opts) {
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h" || arg == "-?") {
            opts.show_help = true;
            return true;
        } else if (arg == "--version" || arg == "-v") {
            opts.show_version = true;
            return true;
        } else if (arg == "-q" || arg == "--brief") {
            opts.format = DiffFormat::Brief;
        } else if (arg == "-u") {
            opts.format = DiffFormat::Unified;
        } else if (arg.rfind("-u=", 0) == 0 || arg.rfind("--unified=", 0) == 0) {
            opts.format = DiffFormat::Unified;
            opts.context_lines = std::stoi(arg.substr(arg.find('=') + 1));
        } else if (arg == "-U" || arg == "--unified") {
            opts.format = DiffFormat::Unified;
            if (i + 1 < argc && std::isdigit(static_cast<unsigned char>(argv[i + 1][0]))) {
                opts.context_lines = std::stoi(argv[++i]);
            }
        } else if (arg.rfind("-U", 0) == 0 && arg.length() > 2) {
            opts.format = DiffFormat::Unified;
            opts.context_lines = std::stoi(arg.substr(2));
        } else if (arg == "-i" || arg == "--ignore-case") {
            opts.ignore_case = true;
        } else if (arg == "-w" || arg == "--ignore-all-space") {
            opts.ignore_all_space = true;
        } else if (arg == "-b" || arg == "--ignore-space-change") {
            opts.ignore_space_change = true;
        } else if (arg == "-B" || arg == "--ignore-blank-lines") {
            opts.ignore_blank_lines = true;
        } else if (arg == "-s" || arg == "--report-identical-files") {
            opts.report_identical = true;
        } else if (arg == "--strip-trailing-cr") {
            opts.strip_trailing_cr = true;
        } else if (arg.length() > 1 && arg[0] == '-' && arg != "-") {
            // Support clustered single-char flags like -qi, -ub
            for (size_t j = 1; j < arg.length(); ++j) {
                switch (arg[j]) {
                    case 'q': opts.format = DiffFormat::Brief; break;
                    case 'u': opts.format = DiffFormat::Unified; break;
                    case 'i': opts.ignore_case = true; break;
                    case 'w': opts.ignore_all_space = true; break;
                    case 'b': opts.ignore_space_change = true; break;
                    case 'B': opts.ignore_blank_lines = true; break;
                    case 's': opts.report_identical = true; break;
                    default:
                        std::cerr << "diff: invalid option -- '" << arg[j] << "'\n";
                        return false;
                }
            }
        } else {
            positional.push_back(arg);
        }
    }

    if (opts.show_help || opts.show_version) return true;

    if (positional.size() < 2) {
        std::cerr << "diff: missing operand after '" << (positional.empty() ? "" : positional[0]) << "'\n"
                  << "Try 'diff --help' for more information.\n";
        return false;
    }

    opts.file1 = positional[0];
    opts.file2 = positional[1];
    return true;
}

} // namespace DiffUtil
