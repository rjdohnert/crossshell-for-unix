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
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace dmesg {

void DmesgOptions::printHelp() {
    std::cout << R"HELP(dmesg(1)                 CrossShell for UNIX Reference Manual                   dmesg(1)

    NAME
        dmesg - read recent Windows system events

    SYNOPSIS
        dmesg [OPTIONS]

    DESCRIPTION
        Reads recent Windows Event Log records and presents kernel, driver, and
        Service Control Manager events in a dmesg-like view.

    OPTIONS
        -n, --max-events COUNT       Maximum matching events to print (default: 50).
        -m, --max-scan COUNT         Maximum records to scan (default: 1000).
        -k, --kernel / -K            Include or exclude kernel providers.
        -d, --driver / -D            Include or exclude driver providers.
        -s, --scm / -S               Include or exclude Service Control Manager events.
        -p, --provider-contains TEXT Filter by provider substring.
        -l, --min-level LEVEL        critical, error, warning, info, or verbose.
        -c, --color / -C             Enable or disable ANSI colors.
        --json, --csv, --table       Select structured output.
        --pipe COMMAND               Send output through COMMAND.
        -h, --help                   Display this comprehensive reference manual.
        -V, --version                Display version information and exit.

    EXAMPLES
        dmesg
        dmesg -n 100 --min-level warning
        dmesg --provider-contains Kernel --json
        dmesg -u --table

    EXIT STATUS
        0          Help, version, or successful event-log rendering.
        1          Invalid options, event-log failure, or pipe failure.

    CrossShell for UNIX                                                         dmesg(1)
)HELP";
}

void DmesgOptions::printVersion() {
    std::cout << "dmesg 1.1.0\n";
}

std::string DmesgOptions::toLowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::wstring DmesgOptions::toWide(const std::string& s) {
    std::wstring out;
    out.reserve(s.size());
    for (unsigned char c : s) out.push_back(static_cast<wchar_t>(c));
    return out;
}

bool DmesgOptions::tryParsePositiveInt(const std::string& text, int& valueOut) {
    if (text.empty()) return false;
    try {
        size_t idx = 0;
        int value = std::stoi(text, &idx, 10);
        if (idx != text.size() || value <= 0) return false;
        valueOut = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool DmesgOptions::tryParseLevel(const std::string& text, int& levelOut) {
    const std::string lower = toLowerAscii(text);
    if (lower == "1" || lower == "critical") { levelOut = 1; return true; }
    if (lower == "2" || lower == "error") { levelOut = 2; return true; }
    if (lower == "3" || lower == "warning" || lower == "warn") { levelOut = 3; return true; }
    if (lower == "4" || lower == "info" || lower == "information") { levelOut = 4; return true; }
    if (lower == "5" || lower == "verbose" || lower == "debug") { levelOut = 5; return true; }
    return false;
}

bool DmesgOptions::parse(int argc, char* argv[], DmesgOptions& options) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            printHelp();
            std::exit(0);
        }
        if (arg == "-V" || arg == "--version") {
            printVersion();
            std::exit(0);
        }
        if (arg == "--json") { options.outputFormat = 1; continue; }
        if (arg == "--csv") { options.outputFormat = 2; continue; }
        if (arg == "--table") { options.outputFormat = 3; continue; }
        if (arg == "--pipe" && i + 1 < argc) { options.pipeCommand = argv[++i]; continue; }
        if (arg == "-c" || arg == "--color") { options.useColor = true; continue; }
        if (arg == "-C" || arg == "--no-color") { options.useColor = false; continue; }
        if (arg == "-k" || arg == "--kernel") { options.includeKernel = true; continue; }
        if (arg == "-K" || arg == "--no-kernel") { options.includeKernel = false; continue; }
        if (arg == "-d" || arg == "--driver") { options.includeDriver = true; continue; }
        if (arg == "-D" || arg == "--no-driver") { options.includeDriver = false; continue; }
        if (arg == "-s" || arg == "--scm") { options.includeScm = true; continue; }
        if (arg == "-S" || arg == "--no-scm") { options.includeScm = false; continue; }

        if (arg == "-n" || arg == "--max-events") {
            if (i + 1 >= argc || !tryParsePositiveInt(argv[++i], options.maxEventsToShow)) {
                std::cerr << "Invalid max-events value\n";
                return false;
            }
            continue;
        }

        if (arg == "-m" || arg == "--max-scan") {
            if (i + 1 >= argc || !tryParsePositiveInt(argv[++i], options.maxEventsToScan)) {
                std::cerr << "Invalid max-scan value\n";
                return false;
            }
            continue;
        }

        if (arg == "-p" || arg == "--provider-contains") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << arg << "\n";
                return false;
            }
            options.providerContains = toWide(toLowerAscii(argv[++i]));
            continue;
        }

        if (arg == "-l" || arg == "--min-level") {
            if (i + 1 >= argc || !tryParseLevel(argv[++i], options.minLevel)) {
                std::cerr << "Invalid min-level value\n";
                return false;
            }
            continue;
        }

        std::cerr << "Unknown option: " << arg << "\n";
        return false;
    }

    if (!options.includeKernel && !options.includeDriver && !options.includeScm) {
        std::cerr << "All provider groups disabled (--no-kernel --no-driver --no-scm). Nothing to display.\n";
        return false;
    }
    return true;
}

} // namespace dmesg
