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
 * SINGLE FILE INDEX: which.cpp
 * ============================================================================
 * WinWhich - Object-Oriented Executable PATH Locator for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. WhichOptions class (CLI parsing & flags)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... WhichReporter class (JSON/CSV/Table/Pipe)
 * 3. [EXECUTABLE SEARCH ENGINE] ............ PathCandidateResolver, WhichEngine classes
 * 4. [APPLICATION CONTROLLER] .............. WhichApp class and main entry point
 * ============================================================================
 */

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>

namespace fs = std::filesystem;

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class WhichOptions {
public:
    bool all{false};
    bool silent{false};
    std::vector<std::string> commands;
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage(const char* progName) {
        std::cout << "Usage: " << progName << " [OPTION]... [--] COMMAND ...\n"
                  << "Show the full path of commands that would be executed.\n\n"
                  << "Options:\n"
                  << "  -a, --all           print all matches in PATH, not just the first\n"
                  << "  -s, --skip-tilde    suppress output and return status only\n"
                  << "      --json, --csv   output structured matches\n"
                  << "      --pipe COMMAND  send output through COMMAND\n"
                  << "  -h, --help          display this help and exit\n"
                  << "      --version       output version information and exit\n";
    }

    static void printVersion() {
        std::cout << "which 1.0\n";
    }

    static bool parse(int argc, char* argv[], WhichOptions& opts) {
        bool parseOptions = true;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (parseOptions && arg == "--") {
                parseOptions = false;
                continue;
            }

            if (parseOptions && (arg == "-h" || arg == "--help" || arg == "/?")) {
                printUsage(argv[0]);
                std::exit(0);
            } else if (parseOptions && arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (parseOptions && (arg == "-a" || arg == "--all")) {
                opts.all = true;
            } else if (parseOptions && (arg == "-s" || arg == "--skip-tilde")) {
                opts.silent = true;
            } else if (parseOptions && arg == "--json") {
                opts.outputFormat = 1;
            } else if (parseOptions && arg == "--csv") {
                opts.outputFormat = 2;
            } else if (parseOptions && arg == "--table") {
                opts.outputFormat = 3;
            } else if (parseOptions && arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (parseOptions && arg[0] == '-' && arg.size() > 1) {
                std::cerr << "which: unknown option: " << arg << "\n";
                return false;
            } else {
                opts.commands.push_back(arg);
            }
        }

        if (opts.commands.empty()) {
            printUsage(argv[0]);
            return false;
        }

        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class WhichReporter {
public:
    static int dispatch(const std::vector<std::pair<std::string, std::string>>& matches,
                        int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"matches\":[";
            for (size_t i = 0; i < matches.size(); ++i) {
                if (i > 0) text += ",";
                text += "{\"command\":\"" + matches[i].first + "\",\"path\":\"" + matches[i].second + "\"}";
            }
            text += "]}\n";
        } else if (format == 2) {
            text = "command,path\n";
            for (const auto& m : matches) {
                text += "\"" + m.first + "\",\"" + m.second + "\"\n";
            }
        } else if (format == 3) {
            text = "COMMAND\tPATH\n--------------------\n";
            for (const auto& m : matches) {
                text += m.first + "\t" + m.second + "\n";
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
// 3. EXECUTABLE SEARCH ENGINE
// ============================================================================

class PathCandidateResolver {
public:
    static std::string toLower(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return str;
    }

    static std::vector<std::string> split(const std::string& str, char delim) {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;
        while (std::getline(ss, token, delim)) {
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }
        return tokens;
    }

    static std::string trimQuotes(std::string str) {
        if (str.length() >= 2 && str.front() == '"' && str.back() == '"') {
            return str.substr(1, str.length() - 2);
        }
        return str;
    }

    static bool isExecutable(const fs::path& p) {
        std::error_code ec;
        return fs::exists(p, ec) && fs::is_regular_file(p, ec);
    }

    static std::vector<fs::path> getCandidates(const fs::path& basePath, const std::vector<std::string>& pathexts) {
        std::vector<fs::path> candidates;
        candidates.push_back(basePath);

        std::string ext = toLower(basePath.extension().string());
        bool hasExt = false;
        for (const auto& pe : pathexts) {
            if (ext == pe) {
                hasExt = true;
                break;
            }
        }

        if (!hasExt) {
            for (const auto& pe : pathexts) {
                fs::path p = basePath;
                p += pe;
                candidates.push_back(p);
            }
        }
        return candidates;
    }
};

class WhichEngine {
private:
    WhichOptions options;

public:
    explicit WhichEngine(WhichOptions opts) : options(std::move(opts)) {}

    int execute() {
        const char* pathEnv = std::getenv("PATH");
        std::vector<std::string> pathDirs;
        if (pathEnv) {
            pathDirs = PathCandidateResolver::split(pathEnv, ';');
        }

        const char* pathextEnv = std::getenv("PATHEXT");
        std::vector<std::string> pathexts;
        if (pathextEnv) {
            pathexts = PathCandidateResolver::split(pathextEnv, ';');
            for (auto& ext : pathexts) {
                ext = PathCandidateResolver::toLower(ext);
            }
        } else {
            pathexts = { ".com", ".exe", ".bat", ".cmd" };
        }

        bool allFound = true;
        std::vector<std::pair<std::string, std::string>> allMatches;

        for (const auto& cmd : options.commands) {
            bool found = false;
            fs::path cmdPath(cmd);

            if (cmd.find('/') != std::string::npos || cmd.find('\\') != std::string::npos) {
                for (const auto& cand : PathCandidateResolver::getCandidates(cmdPath, pathexts)) {
                    if (PathCandidateResolver::isExecutable(cand)) {
                        found = true;
                        std::string full = fs::absolute(cand).lexically_normal().string();
                        if (!options.silent && options.outputFormat == 0 && options.pipeCommand.empty()) {
                            std::cout << full << "\n";
                        }
                        allMatches.emplace_back(cmd, full);
                        if (!options.all) break;
                    }
                }
            } else {
                for (auto dirStr : pathDirs) {
                    dirStr = PathCandidateResolver::trimQuotes(dirStr);
                    if (dirStr.empty()) continue;

                    fs::path dir(dirStr);
                    fs::path base = dir / cmdPath;

                    for (const auto& cand : PathCandidateResolver::getCandidates(base, pathexts)) {
                        if (PathCandidateResolver::isExecutable(cand)) {
                            found = true;
                            std::string full = fs::absolute(cand).lexically_normal().string();
                            if (!options.silent && options.outputFormat == 0 && options.pipeCommand.empty()) {
                                std::cout << full << "\n";
                            }
                            allMatches.emplace_back(cmd, full);
                            if (!options.all) break;
                        }
                    }

                    if (found && !options.all) break;
                }
            }

            if (!found) {
                allFound = false;
            }
        }

        if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
            WhichReporter::dispatch(allMatches, options.outputFormat, options.pipeCommand);
        }

        return allFound ? 0 : 1;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class WhichApp {
public:
    static int run(int argc, char* argv[]) {
        WhichOptions options;
        if (!WhichOptions::parse(argc, argv, options)) {
            return 1;
        }
        WhichEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return WhichApp::run(argc, argv);
}
