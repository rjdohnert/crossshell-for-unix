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
 * SINGLE FILE INDEX: dirname.cpp
 * ============================================================================
 * WinDirname - Object-Oriented Path Directory Extractor for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. DirnameOptions class (CLI parsing & flags)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... DirnameReporter class (JSON/CSV/Table/Pipe)
 * 3. [PATH PARSER ENGINE] .................. DirnameEngine class (UNC, drives, directory trim)
 * 4. [APPLICATION CONTROLLER] .............. DirnameApp class and main entry point
 * ============================================================================
 */

#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class DirnameOptions {
public:
    bool zeroTerminated{false};
    std::vector<std::string> paths;
    int outputFormat{0}; // 0: raw, 1: JSON, 2: CSV, 3: Table
    std::string pipeCommand;

    static void printUsage(const char* /*progName*/ = nullptr) {
        std::cout << R"(dirname(1)                 CrossShell for UNIX Reference Manual                 dirname(1)

    NAME
        dirname - strip last component from file name

    SYNOPSIS
        dirname [OPTION] NAME...

    DESCRIPTION
        Output each NAME with its last non-slash component and trailing slashes
        removed; if NAME contains no /'s or \'s, output '.' (meaning the current
        directory).

    OPTIONS
        -z, --zero
            End each output line with NUL, not newline.
        --json
            Output path records as JSON.
        --csv
            Output path records as CSV.
        --table
            Output path records as a table.
        --pipe COMMAND
            Send output through COMMAND.
        -h, --help
            Display this help and exit.
        --version
            Output version information and exit.

    EXAMPLES
        dirname /usr/bin/sort
            Output "/usr/bin".

        dirname stdio.h
            Output ".".

        dirname /etc/nginx/ /var/log/
            Output "/etc" followed by "/var".

    CrossShell for UNIX                                                    dirname(1)
)";
    }

    static void printVersion() {
        std::cout << "dirname 1.0\n";
    }

    static bool parse(int argc, char* argv[], DirnameOptions& opts) {
        bool stopFlags = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--json") { opts.outputFormat = 1; continue; }
            if (arg == "--csv") { opts.outputFormat = 2; continue; }
            if (arg == "--table") { opts.outputFormat = 3; continue; }
            if (arg == "--pipe" && i + 1 < argc) { opts.pipeCommand = argv[++i]; continue; }

            if (!stopFlags) {
                if (arg == "--") {
                    stopFlags = true;
                    continue;
                } else if (arg == "-h" || arg == "--help" || arg == "/?") {
                    printUsage(argv[0]);
                    std::exit(0);
                } else if (arg == "--version") {
                    printVersion();
                    std::exit(0);
                } else if (arg == "-z" || arg == "--zero" || arg == "-0") {
                    opts.zeroTerminated = true;
                    continue;
                }
            }
            opts.paths.push_back(arg);
        }

        if (opts.paths.empty()) {
            std::cerr << "dirname: missing operand\n";
            std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
            return false;
        }

        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class DirnameReporter {
public:
    static int report(const std::vector<std::string>& results, int format,
                      bool zeroTerminated, const std::string& pipeCommand) {
        if (format != 0 || !pipeCommand.empty()) {
            std::string text;
            if (format == 1) {
                text = "[\n";
                for (size_t i = 0; i < results.size(); ++i) {
                    text += (i ? ",\n" : "") + std::string("{\"value\":\"") + results[i] + "\"}";
                }
                text += "]\n";
            } else if (format == 2) {
                text = "\"dirname\"\n";
                for (const auto& r : results) {
                    text += "\"" + r + "\"\n";
                }
            } else if (format == 3) {
                text = "DIRNAME\n-------\n";
                for (const auto& r : results) {
                    text += r + "\n";
                }
            }

            if (!pipeCommand.empty()) {
                FILE* pipe = _popen(pipeCommand.c_str(), "w");
                if (!pipe) return 1;
                std::fwrite(text.data(), 1, text.size(), pipe);
                _pclose(pipe);
            } else {
                std::cout << text;
            }
        } else {
            for (const auto& dir : results) {
                std::cout << dir;
                if (zeroTerminated) std::cout.put('\0');
                else std::cout << '\n';
            }
        }
        return 0;
    }
};

// ============================================================================
// 3. PATH PARSER ENGINE
// ============================================================================

class DirnameEngine {
private:
    static bool isSlash(char c) {
        return c == '/' || c == '\\';
    }

public:
    static std::string extract(const std::string& path) {
        if (path.empty()) {
            return ".";
        }

        std::string prefix;
        std::string rest = path;

        // 1. Handle UNC Network Paths: \\server\share\...
        if (path.length() >= 2 && isSlash(path[0]) && isSlash(path[1]) && 
            (path.length() == 2 || !isSlash(path[2]))) {
            size_t serverEnd = path.find_first_of("/\\", 2);
            if (serverEnd != std::string::npos) {
                size_t shareEnd = path.find_first_of("/\\", serverEnd + 1);
                if (shareEnd != std::string::npos) {
                    prefix = path.substr(0, shareEnd);
                    rest = path.substr(shareEnd);
                } else {
                    return path;
                }
            } else {
                return path;
            }
        }
        // 2. Handle Drive Letters: C:\... or C:foo
        else if (path.length() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':') {
            prefix = path.substr(0, 2);
            rest = path.substr(2);
        }

        if (rest.empty()) {
            return prefix.empty() ? "." : prefix;
        }

        bool allSlashes = true;
        for (char c : rest) {
            if (!isSlash(c)) {
                allSlashes = false;
                break;
            }
        }

        if (allSlashes) {
            if (!prefix.empty()) {
                return prefix + "\\";
            } else {
                return std::string(1, rest[0]);
            }
        }

        size_t end = rest.length();
        while (end > 0 && isSlash(rest[end - 1])) {
            --end;
        }
        rest = rest.substr(0, end);

        size_t lastSlash = rest.find_last_of("/\\");

        if (lastSlash == std::string::npos) {
            if (!prefix.empty()) {
                return prefix;
            } else {
                return ".";
            }
        }

        size_t dirEnd = lastSlash;
        while (dirEnd > 0 && isSlash(rest[dirEnd - 1])) {
            --dirEnd;
        }

        if (dirEnd == 0) {
            if (!prefix.empty()) {
                return prefix + "\\";
            } else {
                return std::string(1, rest[lastSlash]);
            }
        }

        return prefix + rest.substr(0, dirEnd);
    }

    static int execute(const DirnameOptions& opts) {
        std::vector<std::string> results;
        for (const auto& path : opts.paths) {
            results.push_back(extract(path));
        }
        return DirnameReporter::report(results, opts.outputFormat, opts.zeroTerminated, opts.pipeCommand);
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class DirnameApp {
public:
    static int run(int argc, char* argv[]) {
        DirnameOptions options;
        if (!DirnameOptions::parse(argc, argv, options)) {
            return 1;
        }
        return DirnameEngine::execute(options);
    }
};

int main(int argc, char* argv[]) {
    return DirnameApp::run(argc, argv);
}
