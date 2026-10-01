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
 * SINGLE FILE INDEX: entab.cpp
 * ============================================================================
 * WinEntab - Object-Oriented Space-to-Tab Compressor for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & TAB SPEC PARSER] ........... EntabOptions class (CLI parsing & tab stops)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... EntabReporter class (JSON/CSV/Table/Pipe)
 * 3. [SPACE COMPRESSION ENGINE] ............ EntabEngine class (blank sequence flush & 64KB I/O)
 * 4. [APPLICATION CONTROLLER] .............. EntabApp class and main entry point
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

class EntabOptions {
public:
    static constexpr const char* VERSION = "1.0.0";
    static constexpr size_t IO_BUFFER_SIZE = 64 * 1024;

    bool convertAll{false};          // -a, --all (convert all spaces, not just initial)
    bool explicitFirstOnly{false};   // --first-only (override -a)
    std::vector<size_t> tabStops;    // -t LIST (explicit 1-based column stops)
    size_t singleTabWidth{8};        // -t N or -N (default is 8)
    std::vector<std::string> files;  // File list (or stdin if empty/'-')
    bool showHelp{false};
    bool showVersion{false};
    int outputFormat{0};
    std::string pipeCommand;

    static void printHelp(const char* progName) {
        std::cout << R"(entab(1)                  CrossShell for UNIX Reference Manual                          entab(1)

NAME
     entab - replace groups of spaces with tabs

SYNOPSIS
     entab [OPTION]... [FILE]...

DESCRIPTION
     Convert spaces in each FILE to tabs, writing to standard output.
     With no FILE, or when FILE is -, read standard input.

OPTIONS
     -a, --all
          Convert all sequences of spaces, not just initial whitespace.

     --first-only
          Convert only leading sequences of whitespace (overrides -a).

     -t, --tabs=N
          Have tabs N characters apart (default is 8). Enables -a.

     -t, --tabs=LIST
          Use a comma- or space-separated list of explicit tab stop column
          positions (1-based). Enables -a.

     -N
          Shorthand for -t N (e.g., -4 sets tabs 4 spaces apart).

     -h, --help
          Display this reference manual page and exit.

     -v, --version
          Output version information and exit.

     --json
          Output converted text as JSON.

     --csv
          Output converted text as CSV.

     --table
          Output converted text as a table.

     --pipe COMMAND
          Send converted text through COMMAND.

     --
          Explicitly treat all subsequent arguments as input file names, even
          if they start with '-'.

EXAMPLES
     entab file.txt
     entab -4 data.txt
     entab --first-only source.txt > output.txt
     entab --json report.txt

CrossShell for UNIX                                               entab(1)
)";
    }

    static void printVersion() {
        std::cout << "unexpand version " << VERSION << "\n"
                  << "Copyright (C) 2026 Roberto J Dohnert\n";
    }

    static bool parseTabSpec(const std::string& spec, EntabOptions& opts) {
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

    static bool parse(int argc, char* argv[], EntabOptions& opts) {
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

            if (arg == "-a" || arg == "--all") {
                opts.convertAll = true;
                opts.explicitFirstOnly = false;
                continue;
            }

            if (arg == "--first-only") {
                opts.convertAll = false;
                opts.explicitFirstOnly = true;
                continue;
            }

            if (arg.rfind("--tabs=", 0) == 0) {
                if (!parseTabSpec(arg.substr(7), opts)) {
                    std::cerr << "unexpand: invalid tab spec: '" << arg.substr(7) << "'\n";
                    return false;
                }
                if (!opts.explicitFirstOnly) opts.convertAll = true;
                continue;
            }

            if (arg == "-t" || arg == "--tabs") {
                if (i + 1 >= argc) {
                    std::cerr << "unexpand: option requires an argument -- '" << arg << "'\n";
                    return false;
                }
                if (!parseTabSpec(argv[++i], opts)) {
                    std::cerr << "unexpand: invalid tab spec: '" << argv[i] << "'\n";
                    return false;
                }
                if (!opts.explicitFirstOnly) opts.convertAll = true;
                continue;
            }

            if (arg.rfind("-t", 0) == 0 && arg.size() > 2) {
                if (!parseTabSpec(arg.substr(2), opts)) {
                    std::cerr << "unexpand: invalid tab spec: '" << arg.substr(2) << "'\n";
                    return false;
                }
                if (!opts.explicitFirstOnly) opts.convertAll = true;
                continue;
            }

            if (arg.size() > 1 && arg[0] == '-' && std::isdigit(static_cast<unsigned char>(arg[1]))) {
                if (!parseTabSpec(arg.substr(1), opts)) {
                    std::cerr << "unexpand: invalid tab size: '" << arg.substr(1) << "'\n";
                    return false;
                }
                if (!opts.explicitFirstOnly) opts.convertAll = true;
                continue;
            }

            if (arg.size() > 0 && arg[0] == '-' && arg != "-") {
                std::cerr << "unexpand: invalid option -- '" << arg << "'\n";
                std::cerr << "Try 'unexpand --help' for more information.\n";
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

class EntabReporter {
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
// 3. SPACE COMPRESSION ENGINE
// ============================================================================

class EntabEngine {
private:
    EntabOptions options;

    size_t getNextTabStop(size_t curCol) const {
        if (options.tabStops.empty()) {
            size_t w = options.singleTabWidth;
            if (w == 0) w = 8;
            return ((curCol / w) + 1) * w;
        } else {
            for (size_t stop : options.tabStops) {
                if (stop > curCol) {
                    return stop;
                }
            }
            return SIZE_MAX;
        }
    }

    bool flushPendingBlanks(size_t startCol, size_t endCol, bool hadTab, std::string* capture) const {
        size_t cur = startCol;

        auto writeChar = [capture](int value) {
            return capture ? (capture->push_back(static_cast<char>(value)), value) : fputc(value, stdout);
        };

        while (cur < endCol) {
            size_t nextCol = getNextTabStop(cur);

            if (nextCol <= endCol) {
                size_t width = nextCol - cur;
                if (width > 1 || hadTab) {
                    if (writeChar('\t') == EOF) return false;
                    cur = nextCol;
                } else {
                    if (writeChar(' ') == EOF) return false;
                    cur++;
                }
            } else {
                size_t spacesToEmit = endCol - cur;
                for (size_t i = 0; i < spacesToEmit; ++i) {
                    if (writeChar(' ') == EOF) return false;
                }
                cur = endCol;
            }
        }
        return true;
    }

    bool processStream(FILE* fp, std::string* capture) const {
        setvbuf(fp, NULL, _IOFBF, EntabOptions::IO_BUFFER_SIZE);

        size_t col = 0;
        bool inInitialBlanks = true;
        bool inPendingRun = false;
        size_t pendingStartCol = 0;
        bool pendingHadTab = false;

        auto writeChar = [capture](int value) {
            return capture ? (capture->push_back(static_cast<char>(value)), value) : fputc(value, stdout);
        };

        int ch = 0;
        while ((ch = fgetc(fp)) != EOF) {
            if (ch == ' ' || ch == '\t') {
                bool convert = options.convertAll || inInitialBlanks;

                if (convert) {
                    if (!inPendingRun) {
                        inPendingRun = true;
                        pendingStartCol = col;
                        pendingHadTab = false;
                    }
                    if (ch == '\t') {
                        pendingHadTab = true;
                        size_t nextStop = getNextTabStop(col);
                        col = (nextStop != SIZE_MAX) ? nextStop : (col + 1);
                    } else {
                        col++;
                    }
                } else {
                    if (inPendingRun) {
                        if (!flushPendingBlanks(pendingStartCol, col, pendingHadTab, capture)) return false;
                        inPendingRun = false;
                    }
                    if (writeChar(ch) == EOF) return false;
                    col++;
                }
            } else {
                if (inPendingRun) {
                    if (!flushPendingBlanks(pendingStartCol, col, pendingHadTab, capture)) return false;
                    inPendingRun = false;
                }

                if (writeChar(ch) == EOF) return false;

                if (ch == '\n') {
                    col = 0;
                    inInitialBlanks = true;
                } else if (ch == '\r') {
                    col = 0;
                } else if (ch == '\b') {
                    if (col > 0) col--;
                } else {
                    col++;
                    inInitialBlanks = false;
                }
            }
        }

        if (inPendingRun) {
            if (!flushPendingBlanks(pendingStartCol, col, pendingHadTab, capture)) return false;
        }

        return true;
    }

public:
    explicit EntabEngine(EntabOptions opts) : options(std::move(opts)) {}

    int execute(const char* progName) {
        if (options.showHelp) {
            EntabOptions::printHelp(progName);
            return 0;
        }

        if (options.showVersion) {
            EntabOptions::printVersion();
            return 0;
        }

        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
        setvbuf(stdout, NULL, _IOFBF, EntabOptions::IO_BUFFER_SIZE);

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
                    std::cerr << "unexpand: " << filename << ": No such file or directory\n";
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
            EntabReporter::dispatch(captured, options.outputFormat, options.pipeCommand);
        }
        fflush(stdout);
        return success ? 0 : 1;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class EntabApp {
public:
    static int run(int argc, char* argv[]) {
        if (argc == 1 && _isatty(_fileno(stdin))) {
            EntabOptions::printHelp(argv[0]);
            return 0;
        }

        EntabOptions options;
        if (!EntabOptions::parse(argc, argv, options)) {
            return 1;
        }

        EntabEngine engine(std::move(options));
        return engine.execute(argv[0]);
    }
};

int main(int argc, char* argv[]) {
    return EntabApp::run(argc, argv);
}