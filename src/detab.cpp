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
 * SINGLE FILE INDEX: detab.cpp
 * ============================================================================
 * WinDetab - Object-Oriented Tab-to-Space Expander for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & TAB SPEC PARSER] ........... DetabOptions class (CLI parsing & tab stops)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... DetabReporter class (JSON/CSV/Table/Pipe)
 * 3. [TAB EXPANSION ENGINE] ................ DetabEngine class (column tracking, 64KB buffer)
 * 4. [APPLICATION CONTROLLER] .............. DetabApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <memory>

// ============================================================================
// 1. OPTIONS & TAB SPEC PARSER
// ============================================================================

class DetabOptions {
public:
    static constexpr const char* VERSION = "1.0.0";
    static constexpr size_t IO_BUFFER_SIZE = 64 * 1024;

    bool initialOnly{false};         // -i, --initial
    std::vector<size_t> tabStops;    // -t LIST (explicit 1-based column stops)
    size_t singleTabWidth{8};        // -t N or -N (default is 8)
    std::vector<std::string> files;  // File list (or stdin if empty/'-')
    bool showHelp{false};
    bool showVersion{false};
    int outputFormat{0};
    std::string pipeCommand;

    static void printHelp(const char* progName = nullptr) {
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

    static void printVersion() {
        std::cout << "detab version " << VERSION << "\n"
                  << "Copyright (C) 2026 Roberto J Dohnert\n";
    }

    static bool parseTabSpec(const std::string& spec, DetabOptions& opts) {
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

    static bool parse(int argc, char* argv[], DetabOptions& opts) {
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
            if (arg == "--json") { opts.outputFormat = 1; continue; }
            if (arg == "--csv") { opts.outputFormat = 2; continue; }
            if (arg == "--table") { opts.outputFormat = 3; continue; }
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
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class DetabReporter {
public:
    static int dispatch(const std::string& content, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"output\":\"" + content + "\"}\n";
        } else if (format == 2) {
            text = "\"output\"\n\"" + content + "\"\n";
        } else if (format == 3) {
            text = "OUTPUT\n------\n" + content;
        } else {
            text = content;
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _popen(pipeCommand.c_str(), "w");
            if (!pipe) return 1;
            std::fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
        } else {
            std::cout << text;
        }
        return 0;
    }
};

// ============================================================================
// 3. TAB EXPANSION ENGINE
// ============================================================================

class DetabEngine {
private:
    DetabOptions options;

    size_t calculateSpaces(size_t col) const {
        if (options.tabStops.empty()) {
            size_t w = options.singleTabWidth;
            if (w == 0) return 1;
            return w - (col % w);
        } else {
            for (size_t stop : options.tabStops) {
                if (stop > col + 1) {
                    return (stop - 1) - col;
                }
            }
            return 1;
        }
    }

    bool processStream(FILE* fp, std::string* capture) const {
        size_t col = 0;
        bool inInitialBlanks = true;

        setvbuf(fp, NULL, _IOFBF, DetabOptions::IO_BUFFER_SIZE);

        auto writeChar = [capture](int value) {
            return capture ? (capture->push_back(static_cast<char>(value)), value) : fputc(value, stdout);
        };

        int ch = 0;
        while ((ch = fgetc(fp)) != EOF) {
            if (ch == '\n') {
                if (writeChar(ch) == EOF) return false;
                col = 0;
                inInitialBlanks = true;
            } else if (ch == '\r') {
                if (writeChar(ch) == EOF) return false;
                col = 0;
            } else if (ch == '\b') {
                if (writeChar(ch) == EOF) return false;
                if (col > 0) col--;
            } else if (ch == '\t') {
                if (!options.initialOnly || inInitialBlanks) {
                    size_t numSpaces = calculateSpaces(col);
                    for (size_t i = 0; i < numSpaces; ++i) {
                        if (writeChar(' ') == EOF) return false;
                    }
                    col += numSpaces;
                } else {
                    if (writeChar('\t') == EOF) return false;
                    col += calculateSpaces(col);
                }
            } else {
                if (writeChar(ch) == EOF) return false;
                col++;
                if (ch != ' ') {
                    inInitialBlanks = false;
                }
            }
        }
        return true;
    }

public:
    explicit DetabEngine(DetabOptions opts) : options(std::move(opts)) {}

    int execute(const char* progName) {
        if (options.showHelp) {
            DetabOptions::printHelp(progName);
            return 0;
        }

        if (options.showVersion) {
            DetabOptions::printVersion();
            return 0;
        }

        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
        setvbuf(stdout, NULL, _IOFBF, DetabOptions::IO_BUFFER_SIZE);

        if (options.files.empty()) {
            options.files.push_back("-");
        }

        bool success = true;
        std::string captured;
        std::string* capturePtr = (options.outputFormat || !options.pipeCommand.empty()) ? &captured : nullptr;

        for (const std::string& filename : options.files) {
            FILE* fp = nullptr;

            if (filename == "-") {
                fp = stdin;
            } else {
                errno_t err = fopen_s(&fp, filename.c_str(), "rb");
                if (err != 0 || !fp) {
                    std::cerr << "expand: " << filename << ": No such file or directory\n";
                    success = false;
                    continue;
                }
            }

            if (!processStream(fp, capturePtr)) {
                if (fp != stdin) fclose(fp);
                return 0;
            }

            if (fp != stdin) {
                fclose(fp);
            }
        }

        if (options.outputFormat || !options.pipeCommand.empty()) {
            DetabReporter::dispatch(captured, options.outputFormat, options.pipeCommand);
        }
        fflush(stdout);
        return success ? 0 : 1;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class DetabApp {
public:
    static int run(int argc, char* argv[]) {
        if (argc == 1 && _isatty(_fileno(stdin))) {
            DetabOptions::printHelp(argv[0]);
            return 0;
        }

        DetabOptions options;
        if (!DetabOptions::parse(argc, argv, options)) {
            return 1;
        }

        DetabEngine engine(std::move(options));
        return engine.execute(argv[0]);
    }
};

int main(int argc, char* argv[]) {
    return DetabApp::run(argc, argv);
}