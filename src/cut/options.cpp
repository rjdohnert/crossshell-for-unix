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
#include "range_parser.hpp"
#include <iostream>

void OptionParser::DisplayHelp() {
    std::cout << R"(cut(1)                  CrossShell for UNIX Reference Manual                  cut(1)

    NAME
        cut - remove sections from each line of files

    SYNOPSIS
        cut OPTION... [FILE...]

    DESCRIPTION
        Print selected parts of lines from each FILE to standard output.
        With no FILE, or when FILE is -, read standard input.

    OPTIONS
        -b, --bytes LIST
            Select only these bytes.

        -c, --characters LIST
            Select only these characters.

        -d, --delimiter DELIM
            Use DELIM instead of TAB for field delimiter.

        -f, --fields LIST
            Select only these fields; also print any line that contains no
            delimiter character, unless the -s option is specified.

        -s, --only-delimited
            Do not print lines not containing delimiters.

        --complement
            Complement the set of selected bytes, characters, or fields.

        --output-delimiter STRING
            Use STRING as the output delimiter (default: input delimiter).

        --json, --csv, --table
            Output in JSON, CSV, or tabular format.

        --pipe COMMAND
            Send output directly through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        cut -d: -f1 /etc/passwd
            Output first field (usernames) separated by colon.

        cut -c1-10 file.txt
            Output characters 1 through 10 of each line.

        cut -d, -f2,4 --output-delimiter=TAB data.csv
            Extract columns 2 and 4 from CSV.

    CrossShell for UNIX                                                    cut(1)
)";
}

void OptionParser::DisplayVersion() {
    std::cout << "cut (Extended Utilities) 2.0\n"
              << "Copyright (C) 2026 Roberto J. Dohnert\n";
}

bool OptionParser::Parse(int argc, char* argv[], CutOptions& opts, bool& exitEarly) const {
    exitEarly = false;
    std::string b_list, c_list, f_list;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            DisplayHelp();
            exitEarly = true;
            return true;
        }
        if (arg == "--version" || arg == "-V") {
            DisplayVersion();
            exitEarly = true;
            return true;
        }
        if (arg == "--json") { opts.output_format = 1; continue; }
        if (arg == "--csv") { opts.output_format = 2; continue; }
        if (arg == "--table") { opts.output_format = 3; continue; }
        if (arg == "--pipe" && i + 1 < argc) { opts.pipe_command = argv[++i]; continue; }
        if (arg == "--complement") { opts.complement = true; continue; }

        if (arg.rfind("--output-delimiter=", 0) == 0) {
            opts.output_delimiter = arg.substr(19);
            continue;
        }
        if (arg == "--output-delimiter") {
            if (i + 1 >= argc) {
                std::cerr << "cut: option requires an argument -- output-delimiter\n";
                return false;
            }
            opts.output_delimiter = argv[++i];
            continue;
        }

        if (arg.rfind("--delimiter=", 0) == 0) {
            opts.delimiter = arg.substr(12);
            continue;
        }
        if (arg.rfind("--fields=", 0) == 0) {
            f_list = arg.substr(9);
            continue;
        }
        if (arg.rfind("--bytes=", 0) == 0) {
            b_list = arg.substr(8);
            continue;
        }
        if (arg.rfind("--characters=", 0) == 0) {
            c_list = arg.substr(13);
            continue;
        }

        if (arg.rfind("-", 0) == 0 && arg.size() > 1 && arg != "-") {
            char opt = arg[1];
            std::string val;

            if (arg.size() > 2) {
                val = arg.substr(2);
            } else if (opt != 's') {
                if (i + 1 < argc) {
                    val = argv[++i];
                } else {
                    std::cerr << "cut: option requires an argument -- " << opt << "\n";
                    return false;
                }
            }

            if (opt == 'b') b_list = val;
            else if (opt == 'c') c_list = val;
            else if (opt == 'f') f_list = val;
            else if (opt == 'd') {
                if (val.empty()) {
                    std::cerr << "cut: bad delimiter\n";
                    return false;
                }
                opts.delimiter = val;
            }
            else if (opt == 's') opts.only_delimited = true;
            else {
                std::cerr << "cut: unknown option -- " << opt << "\n";
                return false;
            }
        } else {
            opts.files.push_back(arg);
        }
    }

    int modes = (b_list.empty() ? 0 : 1) + (c_list.empty() ? 0 : 1) + (f_list.empty() ? 0 : 1);
    if (modes != 1) {
        std::cerr << "cut: must specify only one of -b, -c, or -f\n";
        return false;
    }

    if (!b_list.empty()) {
        opts.mode = CutMode::BYTES;
        opts.list_str = b_list;
    } else if (!c_list.empty()) {
        opts.mode = CutMode::CHARACTERS;
        opts.list_str = c_list;
    } else {
        opts.mode = CutMode::FIELDS;
        opts.list_str = f_list;
    }

    if (!RangeParser::ParseList(opts.list_str, opts.selection)) {
        std::cerr << "cut: " << opts.list_str << ": invalid list value\n";
        return false;
    }

    return true;
}
