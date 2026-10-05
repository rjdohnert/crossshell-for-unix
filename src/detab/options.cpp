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
#include <cctype>

namespace detab {

void DetabOptions::printHelp(const char* progName) {
    (void)progName;
    std::cout << R"(detab(1)                CrossShell for UNIX Reference Manual                  detab(1)

    NAME
        detab - replace tab characters with spaces

    SYNOPSIS
        detab [OPTIONS] [FILE...]

    DESCRIPTION
        Convert tabs in each FILE to spaces, writing to standard output.
        With no FILE, or when FILE is -, read standard input.

    OPTIONS
        -i, --initial
            Do not convert tabs after non-blank characters.

        -t, --tabs N
            Have tabs N characters apart (default is 8).

        -t, --tabs LIST
            Use a comma- or space-separated list of explicit tab stop column
            positions (1-based).

        -N
            Shorthand for -t N (e.g., -4 sets tabs 4 spaces apart).

        --json, --csv, --table
            Output transformed text in JSON, CSV, or tabular format.

        --pipe COMMAND
            Send transformed output directly through COMMAND.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version and license information.

        --
            Explicitly treat all subsequent arguments as input file names.

    EXAMPLES
        detab file.txt
            Expand tabs using default 8-space tabs.

        detab -4 data.txt
            Expand tabs using 4-space tab stops.

        detab -i source.txt > output.txt
            Expand leading tabs only.

    CrossShell for UNIX                                                    detab(1)
)";
}

void DetabOptions::printVersion() {
    std::cout << "detab version " << VERSION << "\n"
              << "Copyright (C) 2026 Roberto J Dohnert\n";
}

bool DetabOptions::parseTabSpec(const std::string& spec, DetabOptions& opts) {
    std::vector<size_t> stops;
    std::string token;

    for (char c : spec) {
        if (c == ',' || c == ' ' || c == '\t') {
            if (!token.empty()) {
                try {
                    unsigned long long val = std::stoull(token);
                    if (val == 0) return false;
                    stops.push_back(static_cast<size_t>(val));
                } catch (...) {
                    return false;
                }
                token.clear();
            }
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            token += c;
        } else {
            return false;
        }
    }

    if (!token.empty()) {
        try {
            unsigned long long val = std::stoull(token);
            if (val == 0) return false;
            stops.push_back(static_cast<size_t>(val));
        } catch (...) {
            return false;
        }
    }

    if (stops.empty()) return false;

    if (stops.size() == 1) {
        opts.singleTabWidth = stops[0];
        opts.tabStops.clear();
    } else {
        opts.tabStops = stops;
        opts.singleTabWidth = 8;
    }
    return true;
}

bool DetabOptions::parse(int argc, char* argv[], DetabOptions& opts) {
    bool rawFilesOnly = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (rawFilesOnly) {
            opts.files.push_back(arg);
            continue;
        }

        if (arg == "--") {
            rawFilesOnly = true;
            continue;
        }

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            opts.showHelp = true;
            return true;
        }

        if (arg == "-v" || arg == "--version") {
            opts.showVersion = true;
            return true;
        }
        if (arg == "--json") { opts.outputFormat = OutputFormat::Json; continue; }
        if (arg == "--csv") { opts.outputFormat = OutputFormat::Csv; continue; }
        if (arg == "--table") { opts.outputFormat = OutputFormat::Table; continue; }
        if (arg == "--pipe" && i + 1 < argc) { opts.pipeCommand = argv[++i]; continue; }

        if (arg == "-i" || arg == "--initial") {
            opts.initialOnly = true;
            continue;
        }

        if (arg.rfind("--tabs=", 0) == 0) {
            if (!parseTabSpec(arg.substr(7), opts)) {
                std::cerr << "expand: invalid tab spec: '" << arg.substr(7) << "'\n";
                return false;
            }
            continue;
        }

        if (arg == "-t" || arg == "--tabs") {
            if (i + 1 >= argc) {
                std::cerr << "expand: option requires an argument -- '" << arg << "'\n";
                return false;
            }
            if (!parseTabSpec(argv[++i], opts)) {
                std::cerr << "expand: invalid tab spec: '" << argv[i] << "'\n";
                return false;
            }
            continue;
        }

        if (arg.rfind("-t", 0) == 0 && arg.size() > 2) {
            if (!parseTabSpec(arg.substr(2), opts)) {
                std::cerr << "expand: invalid tab spec: '" << arg.substr(2) << "'\n";
                return false;
            }
            continue;
        }

        if (arg.size() > 1 && arg[0] == '-' && std::isdigit(static_cast<unsigned char>(arg[1]))) {
            if (!parseTabSpec(arg.substr(1), opts)) {
                std::cerr << "expand: invalid tab size: '" << arg.substr(1) << "'\n";
                return false;
            }
            continue;
        }

        if (arg.size() > 0 && arg[0] == '-' && arg != "-") {
            std::cerr << "expand: invalid option -- '" << arg << "'\n";
            std::cerr << "Try 'expand --help' for more information.\n";
            return false;
        }

        opts.files.push_back(arg);
    }

    return true;
}

} // namespace detab
