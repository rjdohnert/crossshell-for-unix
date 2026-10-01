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
 * SINGLE FILE INDEX: head.cpp
 * ============================================================================
 * WinHead - Object-Oriented Beginning-of-Stream / File Viewer for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. HeadOptions class (CLI parsing & options)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... HeadReporter class (JSON/CSV/Table/Pipe)
 * 3. [STREAM PROCESSOR ENGINE] ............. HeadEngine class (line/byte streaming & headers)
 * 4. [APPLICATION CONTROLLER] .............. HeadApp class and main entry point
 * ============================================================================
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <cctype>
#include <cstdio>
#include <memory>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class HeadOptions {
public:
    long long lineCount{10};
    long long byteCount{0};
    bool useBytes{false};
    bool quiet{false};
    bool verbose{false};
    int outputFormat{0}; // 0: raw, 1: JSON, 2: CSV, 3: Table
    std::string pipeCommand;
    std::vector<std::string> files;

    static void printUsage(const char* /*progName*/) {
        std::cout << R"HELP(head(1)                 CrossShell for UNIX Reference Manual                  head(1)

    NAME
        head - Output the first part of files or standard input.

    SYNOPSIS
        head [OPTIONS] [FILE...]

    DESCRIPTION
        Prints the first 10 lines of each FILE to standard output. A line or
        byte limit may be selected. With multiple files, each result is
        preceded by a file-name header unless quiet mode is enabled. When no
        FILE is supplied, or FILE is -, input is read from standard input.

    OPTIONS
        -n, --lines NUM
            Print the first NUM lines instead of 10.

        -NUM
            Print the first NUM lines using the traditional shorthand.

        -c, --bytes NUM
            Print the first NUM bytes of each file.

        -q, --quiet
            Suppress file-name headers.

        -v, --verbose
            Always print file-name headers.

        --json
            Format selected content as JSON.

        --csv
            Format selected content as CSV.

        --table
            Format selected content as a table.

        --pipe COMMAND
            Send output through COMMAND.

        --
            End option processing; treat remaining arguments as file names.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        head document.txt
            Display the first 10 lines of document.txt.

        head -n 20 document.txt
            Display the first 20 lines of document.txt.

        head -c 512 binary.dat
            Display the first 512 bytes of binary.dat.

        head -n 5 file1.txt file2.txt
            Display the first 5 lines of each file with file headers.

        type document.txt | head -5
            Display the first five lines received from standard input.

        head --json -n 5 document.txt
            Format the selected content as JSON.

    CrossShell for UNIX                                                        head(1)
)HELP";
    }

    static void printVersion() {
        std::cout << "head 1.0\n";
    }

    static bool parseInt(const std::string& str, long long& val) {
        if (str.empty()) return false;
        std::stringstream ss(str);
        ss >> val;
        return !ss.fail() && ss.eof() && val >= 0;
    }

    static bool parse(int argc, char* argv[], HeadOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--") {
                for (int j = i + 1; j < argc; ++j) {
                    opts.files.push_back(argv[j]);
                }
                break;
            } else if (arg == "-") {
                opts.files.push_back("-");
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg == "--help" || arg == "-h" || arg == "/?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == "--version" || arg == "-V") {
                printVersion();
                std::exit(0);
            } else if (arg == "-q" || arg == "--quiet") {
                opts.quiet = true;
                opts.verbose = false;
            } else if (arg == "-v" || arg == "--verbose") {
                opts.verbose = true;
                opts.quiet = false;
            } else if (arg.rfind("--lines=", 0) == 0) {
                std::string valStr = arg.substr(8);
                long long val = 0;
                if (!parseInt(valStr, val)) {
                    std::cerr << "head: illegal line count -- " << valStr << "\n";
                    return false;
                }
                opts.lineCount = val;
                opts.useBytes = false;
            } else if (arg.rfind("--bytes=", 0) == 0) {
                std::string valStr = arg.substr(8);
                long long val = 0;
                if (!parseInt(valStr, val)) {
                    std::cerr << "head: illegal byte count -- " << valStr << "\n";
                    return false;
                }
                opts.byteCount = val;
                opts.useBytes = true;
            } else if (arg == "-n" || arg == "--lines") {
                if (i + 1 >= argc) {
                    std::cerr << "head: option requires an argument -- n\n";
                    printUsage(argv[0]);
                    return false;
                }
                long long val = 0;
                if (!parseInt(argv[++i], val)) {
                    std::cerr << "head: illegal line count -- " << argv[i] << "\n";
                    return false;
                }
                opts.lineCount = val;
                opts.useBytes = false;
            } else if (arg == "-c" || arg == "--bytes") {
                if (i + 1 >= argc) {
                    std::cerr << "head: option requires an argument -- c\n";
                    printUsage(argv[0]);
                    return false;
                }
                long long val = 0;
                if (!parseInt(argv[++i], val)) {
                    std::cerr << "head: illegal byte count -- " << argv[i] << "\n";
                    return false;
                }
                opts.byteCount = val;
                opts.useBytes = true;
            } else if (arg[0] == '-' && arg.size() > 1) {
                if (std::isdigit(static_cast<unsigned char>(arg[1]))) {
                    long long val = 0;
                    if (parseInt(arg.substr(1), val)) {
                        opts.lineCount = val;
                        opts.useBytes = false;
                    } else {
                        std::cerr << "head: illegal line count -- " << arg.substr(1) << "\n";
                        return false;
                    }
                } else {
                    for (size_t j = 1; j < arg.size(); ++j) {
                        char option = arg[j];
                        if (option == 'h') {
                            printUsage(argv[0]);
                            std::exit(0);
                        } else if (option == 'q') {
                            opts.quiet = true;
                            opts.verbose = false;
                        } else if (option == 'v') {
                            opts.verbose = true;
                            opts.quiet = false;
                        } else if (option == 'n' || option == 'c') {
                            std::string valStr;
                            if (j + 1 < arg.size()) {
                                valStr = arg.substr(j + 1);
                                j = arg.size();
                            } else if (i + 1 < argc) {
                                valStr = argv[++i];
                            } else {
                                std::cerr << "head: option requires an argument -- " << option << "\n";
                                printUsage(argv[0]);
                                return false;
                            }
                            long long val = 0;
                            if (!parseInt(valStr, val)) {
                                std::cerr << "head: illegal " << (option == 'n' ? "line" : "byte") << " count -- " << valStr << "\n";
                                return false;
                            }
                            if (option == 'n') {
                                opts.lineCount = val;
                                opts.useBytes = false;
                            } else {
                                opts.byteCount = val;
                                opts.useBytes = true;
                            }
                        } else {
                            std::cerr << "head: unknown option -- " << option << "\n";
                            printUsage(argv[0]);
                            return false;
                        }
                    }
                }
            } else {
                opts.files.push_back(arg);
            }
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

class HeadReporter {
public:
    static int dispatch(const std::string& content, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"output\":\"" + content + "\"}\n";
        } else if (format == 2) {
            text = "output\n\"" + content + "\"\n";
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
// 3. STREAM PROCESSOR ENGINE
// ============================================================================

class HeadEngine {
private:
    HeadOptions options;

public:
    explicit HeadEngine(HeadOptions opts) : options(std::move(opts)) {}

    static void stream(std::istream& in, long long limit, bool useBytes, std::ostream& out) {
        if (useBytes) {
            if (limit <= 0) return;
            char buffer[4096];
            long long remaining = limit;
            while (remaining > 0 && in) {
                long long toRead = (remaining < 4096) ? remaining : 4096;
                in.read(buffer, toRead);
                std::streamsize bytesRead = in.gcount();
                if (bytesRead > 0) {
                    out.write(buffer, bytesRead);
                    remaining -= bytesRead;
                } else {
                    break;
                }
            }
        } else {
            if (limit <= 0) return;
            char ch;
            long long count = 0;
            while (count < limit && in.get(ch)) {
                out.put(ch);
                if (ch == '\n') {
                    count++;
                }
            }
        }
    }

    int execute() {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif

        bool showHeaders = false;
        if (options.verbose) {
            showHeaders = true;
        } else if (options.quiet) {
            showHeaders = false;
        } else {
            showHeaders = (options.files.size() > 1);
        }

        bool success = true;
        std::ostringstream captured;
        std::ostream* outStream = (options.outputFormat || !options.pipeCommand.empty()) ? &captured : &std::cout;
        bool firstOutput = true;

        for (const auto& file : options.files) {
            long long limit = options.useBytes ? options.byteCount : options.lineCount;

            if (file == "-") {
                if (!firstOutput && showHeaders) {
                    *outStream << "\n";
                }
                if (showHeaders) {
                    *outStream << "==> standard input <==\n";
                }
                stream(std::cin, limit, options.useBytes, *outStream);
                firstOutput = false;
            } else {
                std::ifstream infile(file, std::ios_base::in | std::ios_base::binary);
                if (!infile.is_open()) {
                    std::cerr << "head: " << file << ": No such file or directory\n";
                    success = false;
                    continue;
                }
                if (!firstOutput && showHeaders) {
                    *outStream << "\n";
                }
                if (showHeaders) {
                    *outStream << "==> " << file << " <==\n";
                }
                stream(infile, limit, options.useBytes, *outStream);
                firstOutput = false;
            }
        }

        if (options.outputFormat || !options.pipeCommand.empty()) {
            HeadReporter::dispatch(captured.str(), options.outputFormat, options.pipeCommand);
        }

        return success ? 0 : 1;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class HeadApp {
public:
    static int run(int argc, char* argv[]) {
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(NULL);

        HeadOptions options;
        if (!HeadOptions::parse(argc, argv, options)) {
            return 1;
        }

        HeadEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return HeadApp::run(argc, argv);
}
