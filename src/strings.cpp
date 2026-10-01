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

/**
 * ============================================================================
 * SINGLE FILE INDEX: strings.cpp
 * ============================================================================
 * WinStrings - Object-Oriented Printable String Extractor for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. StringsOptions class (CLI parsing & options)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... StringsReporter class (JSON/CSV/Table/Pipe)
 * 3. [PRINTABLE STRING EXTRACTOR ENGINE] ... StringsEngine class (byte-stream extraction)
 * 4. [APPLICATION CONTROLLER] .............. StringsApp class and main entry point
 * ============================================================================
 */

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class StringsOptions {
public:
    size_t minLength{4};
    std::vector<std::string> files;
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage(const char* progName) {
        std::cout << R"(strings(1)              CrossShell for UNIX Reference Manual                 strings(1)

    NAME
        strings - print the sequences of printable characters in files

    SYNOPSIS
        strings [OPTIONS] [FILE]...

    DESCRIPTION
        For each FILE, prints the sequences of printable characters that are at
        least 4 characters long (or the number specified by -n). Reads standard
        input when FILE is '-' or omitted.

    OPTIONS
        -n, --bytes N
            Locate and print sequences of at least N printable characters (default: 4).

        --json
            Emit string matches in JSON format.

        --csv
            Emit string matches in CSV format.

        --table
            Emit string matches in tabular format.

        --pipe COMMAND
            Stream results directly into COMMAND.

        -h, --help
            Display this reference manual.

        -v, -V, --version
            Display version and license information.

    EXAMPLES
        strings executable.exe
            Print printable character sequences in executable.exe.

        strings -n 8 /bin/ls
            Find strings at least 8 characters long.

        strings --json libtest.dll
            Extract strings from DLL formatted as JSON records.

    CrossShell for UNIX                                                    strings(1)
    )";
    }

    static void printVersion() {
        std::cout << "strings v1.0.0\n";
    }

    static bool parseUnsigned(const std::string& text, size_t& outValue) {
        if (text.empty()) return false;
        char* end = nullptr;
        unsigned long long value = std::strtoull(text.c_str(), &end, 10);
        if (end == text.c_str() || *end != '\0' || value == 0) return false;
        outValue = static_cast<size_t>(value);
        return true;
    }

    static bool parse(int argc, char* argv[], StringsOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";
            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printUsage(argv[0]);
                std::exit(0);
            }
            if (arg == "--version" || arg == "-V") {
                printVersion();
                std::exit(0);
            }
            if (arg == "--json") { opts.outputFormat = 1; continue; }
            if (arg == "--csv") { opts.outputFormat = 2; continue; }
            if (arg == "--table") { opts.outputFormat = 3; continue; }
            if (arg == "--pipe") {
                if (i + 1 >= argc) return false;
                opts.pipeCommand = argv[++i];
                continue;
            }
            if (arg == "-n" || arg == "--bytes") {
                if (i + 1 >= argc) {
                    std::cerr << "strings: missing value for " << arg << "\n";
                    return false;
                }
                if (!parseUnsigned(argv[++i], opts.minLength)) {
                    std::cerr << "strings: invalid minimum length: " << argv[i] << "\n";
                    return false;
                }
                continue;
            }
            if (!arg.empty() && arg[0] == '-') {
                if (parseUnsigned(arg.substr(1), opts.minLength)) {
                    continue;
                }
                std::cerr << "strings: unrecognized option: " << arg << "\n";
                printUsage(argv[0]);
                return false;
            }
            opts.files.push_back(arg);
        }

        if (opts.files.empty()) {
            opts.files.push_back("-");
        }

        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class StringsReporter {
public:
    static int dispatch(const std::vector<std::string>& results, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "[\n";
            for (size_t i = 0; i < results.size(); ++i) {
                text += (i ? ",\n" : "") + std::string("  {\"string\":\"") + results[i] + "\"}";
            }
            text += "\n]\n";
        } else if (format == 2) {
            text = "string\n";
            for (const auto& s : results) {
                text += "\"" + s + "\"\n";
            }
        } else if (format == 3) {
            text = "STRINGS\n-------\n";
            for (const auto& s : results) {
                text += s + "\n";
            }
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _popen(pipeCommand.c_str(), "w");
            if (!pipe) return 1;
            std::fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
        } else if (format != 0) {
            std::cout << text;
        }
        return 0;
    }
};

// ============================================================================
// 3. PRINTABLE STRING EXTRACTOR ENGINE
// ============================================================================

class StringsEngine {
private:
    StringsOptions options;

    static bool isPrintable(unsigned char c) {
        return (c >= 32 && c <= 126) || c == '\t';
    }

    void processStream(std::istream& in, std::vector<std::string>& results) const {
        std::string current;
        char ch = 0;
        while (in.get(ch)) {
            unsigned char uc = static_cast<unsigned char>(ch);
            if (isPrintable(uc)) {
                current.push_back(ch);
            } else {
                if (current.length() >= options.minLength) {
                    if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
                        results.push_back(current);
                    } else {
                        std::cout << current << "\n";
                    }
                }
                current.clear();
            }
        }

        if (current.length() >= options.minLength) {
            if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
                results.push_back(current);
            } else {
                std::cout << current << "\n";
            }
        }
    }

public:
    explicit StringsEngine(StringsOptions opts) : options(std::move(opts)) {}

    int execute() {
        std::vector<std::string> results;
        bool allOk = true;

        for (const auto& path : options.files) {
            if (path == "-") {
                processStream(std::cin, results);
            } else {
                std::ifstream in(path, std::ios::binary);
                if (!in.is_open()) {
                    std::cerr << "strings: '" << path << "': No such file\n";
                    allOk = false;
                    continue;
                }
                processStream(in, results);
            }
        }

        if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
            StringsReporter::dispatch(results, options.outputFormat, options.pipeCommand);
        }

        return allOk ? 0 : 1;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class StringsApp {
public:
    static int run(int argc, char* argv[]) {
        StringsOptions options;
        if (!StringsOptions::parse(argc, argv, options)) {
            return 1;
        }
        StringsEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return StringsApp::run(argc, argv);
}
