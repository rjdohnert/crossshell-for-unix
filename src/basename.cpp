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
 * SINGLE FILE INDEX: basename.cpp
 * ============================================================================
 * WinBasename - Object-Oriented Path Basename Extractor for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. BasenameOptions class (CLI parsing & flags)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... BasenameReporter class (JSON/CSV/Table/Pipe)
 * 3. [PATH PARSER ENGINE] .................. BasenameEngine class (path stripping & suffix removal)
 * 4. [APPLICATION CONTROLLER] .............. BasenameApp class and main entry point
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

class BasenameOptions {
public:
    bool multiple{false};
    bool zeroTerminated{false};
    std::string suffix{""};
    std::vector<std::string> paths;
    int outputFormat{0}; // 0: raw, 1: JSON, 2: CSV, 3: Table
    std::string pipeCommand;

    static void printUsage(const char* progName) {
        std::cerr << "usage: " << progName << " NAME [SUFFIX]\n";
        std::cerr << "       " << progName << " OPTION... NAME...\n";
    }

    static void printHelp(const char* progName = nullptr) {
        (void)progName;
        std::cout << R"(basename(1)             CrossShell for UNIX Reference Manual           basename(1)

    NAME
        basename - strip directory and suffix from filenames

    SYNOPSIS
        basename NAME [SUFFIX]
        basename OPTION... NAME...

    DESCRIPTION
        Print NAME with any leading directory components removed. If specified,
        also remove a trailing SUFFIX.

    OPTIONS
        -a, --multiple
            Support multiple arguments and treat each as a NAME.

        -s, --suffix SUFFIX
            Remove a trailing SUFFIX; implies -a.

        -z, --zero
            End each output line with NUL, not newline.

        --json, --csv, --table
            Output basename records in JSON, CSV, or tabular format.

        --pipe COMMAND
            Pipe output directly to COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        basename /usr/bin/sort
            Output "sort".

        basename include/stdio.h .h
            Output "stdio".

        basename -s .h include/stdio.h sys/socket.h
            Output "stdio" and "socket".

    CrossShell for UNIX                                                    basename(1)
)";
    }

    static void printVersion() {
        std::cout << "basename 1.0\n";
    }

    static bool parse(int argc, char* argv[], BasenameOptions& opts) {
        int i = 1;
        for (; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--json") { opts.outputFormat = 1; continue; }
            if (arg == "--csv") { opts.outputFormat = 2; continue; }
            if (arg == "--table") { opts.outputFormat = 3; continue; }
            if (arg == "--pipe" && i + 1 < argc) { opts.pipeCommand = argv[++i]; continue; }
            if (arg == "--") {
                i++;
                break;
            } else if (arg == "--help" || arg == "-h" || arg == "/?") {
                printHelp(argv[0]);
                std::exit(0);
            } else if (arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-a" || arg == "--multiple") {
                opts.multiple = true;
            } else if (arg == "-s" || arg == "--suffix") {
                if (i + 1 < argc) {
                    opts.suffix = argv[++i];
                    opts.multiple = true;
                } else {
                    std::cerr << "basename: option requires an argument -- 's'\n";
                    printUsage(argv[0]);
                    return false;
                }
            } else if (arg.rfind("--suffix=", 0) == 0) {
                opts.suffix = arg.substr(9);
                opts.multiple = true;
            } else if (arg == "-z" || arg == "--zero") {
                opts.zeroTerminated = true;
            } else if (arg[0] == '-' && arg.size() > 1) {
                bool valid = true;
                for (size_t j = 1; j < arg.size(); ++j) {
                    if (arg[j] == 'a') {
                        opts.multiple = true;
                    } else if (arg[j] == 's') {
                        if (j + 1 < arg.size()) {
                            opts.suffix = arg.substr(j + 1);
                            opts.multiple = true;
                            break;
                        } else if (i + 1 < argc) {
                            opts.suffix = argv[++i];
                            opts.multiple = true;
                            break;
                        } else {
                            std::cerr << "basename: option requires an argument -- 's'\n";
                            printUsage(argv[0]);
                            return false;
                        }
                    } else if (arg[j] == 'z') {
                        opts.zeroTerminated = true;
                    } else {
                        valid = false;
                        break;
                    }
                }
                if (!valid) {
                    std::cerr << "basename: invalid option -- '" << arg[1] << "'\n";
                    printUsage(argv[0]);
                    return false;
                }
            } else {
                break;
            }
        }

        for (; i < argc; ++i) {
            opts.paths.push_back(argv[i]);
        }

        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class BasenameReporter {
public:
    static int report(const std::vector<std::string>& results, int format,
                      bool zeroTerminated, const std::string& pipeCommand) {
        if (format != 0 || !pipeCommand.empty()) {
            std::string text;
            if (format == 1) {
                text = "[\n";
                for (size_t n = 0; n < results.size(); ++n) {
                    text += (n ? ",\n" : "") + std::string("{\"value\":\"") + results[n] + "\"}";
                }
                text += "\n]\n";
            } else if (format == 2) {
                text = "\"basename\"\n";
                for (const auto& r : results) {
                    text += "\"" + r + "\"\n";
                }
            } else if (format == 3) {
                text = "BASENAME\n--------\n";
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
            for (const auto& result : results) {
                std::cout << result;
                if (zeroTerminated) std::cout.put('\0');
                else std::cout.put('\n');
            }
        }
        return 0;
    }
};

// ============================================================================
// 3. PATH PARSER ENGINE
// ============================================================================

class BasenameEngine {
public:
    static std::string extract(std::string path, const std::string& suffix) {
        if (path.empty()) {
            return "";
        }

        // 1. Strip trailing path separators
        size_t lastNonSlash = path.find_last_not_of("/\\");
        if (lastNonSlash == std::string::npos) {
            return std::string(1, path[0]);
        }
        path = path.substr(0, lastNonSlash + 1);

        // 2. Extract last component
        size_t lastSlash = path.find_last_of("/\\");
        std::string result = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;

        // 3. Handle Windows drive prefixes (e.g. C:foo -> foo)
        if (result.size() >= 2 && result[1] == ':' && std::isalpha(static_cast<unsigned char>(result[0]))) {
            if (result.size() > 2) {
                result = result.substr(2);
            }
        }

        // 4. Remove suffix
        if (!suffix.empty() && result != suffix) {
            if (result.size() >= suffix.size()) {
                size_t suffixPos = result.size() - suffix.size();
                if (result.compare(suffixPos, suffix.size(), suffix) == 0) {
                    result = result.substr(0, suffixPos);
                }
            }
        }

        return result;
    }

    static int execute(const BasenameOptions& opts, const char* progName) {
        std::vector<std::string> results;

        if (opts.multiple) {
            if (opts.paths.empty()) {
                BasenameOptions::printUsage(progName);
                return 1;
            }
            for (const auto& str : opts.paths) {
                results.push_back(extract(str, opts.suffix));
            }
        } else {
            if (opts.paths.empty()) {
                BasenameOptions::printUsage(progName);
                return 1;
            } else if (opts.paths.size() == 1) {
                results.push_back(extract(opts.paths[0], ""));
            } else if (opts.paths.size() == 2) {
                results.push_back(extract(opts.paths[0], opts.paths[1]));
            } else {
                std::cerr << "basename: extra operand '" << opts.paths[2] << "'\n";
                BasenameOptions::printUsage(progName);
                return 1;
            }
        }

        return BasenameReporter::report(results, opts.outputFormat, opts.zeroTerminated, opts.pipeCommand);
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class BasenameApp {
public:
    static int run(int argc, char* argv[]) {
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(NULL);

        BasenameOptions options;
        if (!BasenameOptions::parse(argc, argv, options)) {
            return 1;
        }
        return BasenameEngine::execute(options, argv[0]);
    }
};

int main(int argc, char* argv[]) {
    return BasenameApp::run(argc, argv);
}
