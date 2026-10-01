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
 * SINGLE FILE INDEX: csplit.cpp
 * ============================================================================
 * WinCsplit - Object-Oriented Context-Based File Splitter for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & PATTERN STRUCTURES] ........ SplitPattern and CsplitOptions classes
 * 2. [STRUCTURED OUTPUT REPORTER] .......... CsplitReporter class (JSON/CSV/Table/Pipe)
 * 3. [CONTEXT SPLIT ENGINE] ................ FileSplitManager and CsplitEngine classes
 * 4. [APPLICATION CONTROLLER] .............. CsplitApp class and main entry point
 * ============================================================================
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <sstream>
#include <iomanip>
#include <climits>
#include <cstdio>
#include <algorithm>
#include <memory>

// ============================================================================
// 1. OPTIONS & PATTERN STRUCTURES
// ============================================================================

enum class PatternType {
    LineNum,
    RegexMatch, // /regex/
    RegexSkip   // %regex%
};

struct SplitPattern {
    PatternType type{PatternType::LineNum};
    long long lineNum{0};
    std::string regexStr;
    long long offset{0};
    int repeat{0}; // 0 = run 1 time; N = repeat N times; -1 = {*}
};

class CsplitOptions {
public:
    std::string prefix{"xx"};
    int digits{2};
    bool quiet{false};
    bool keepFiles{false};
    std::string suffixFormat;
    std::string inputFile;
    std::vector<SplitPattern> patterns;
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage(const char* progName = nullptr) {
        (void)progName;
        std::cout << R"(csplit(1)               CrossShell for UNIX Reference Manual           csplit(1)

    NAME
        csplit - split a file into sections determined by context lines

    SYNOPSIS
        csplit [OPTIONS] FILE PATTERN...

    DESCRIPTION
        Output pieces of FILE separated by PATTERN(s) to files 'xx00', 'xx01', ...,
        and output byte counts of each piece to standard output.

    OPTIONS
        -f, --prefix PREFIX
            Use PREFIX instead of 'xx'.

        -n, --digits DIGITS
            Use DIGITS digits instead of default 2.

        -s, -q, --silent, --quiet
            Do not print counts of output file sizes.

        -k, --keep-files
            Do not remove output files on errors.

        -b, --suffix-format FORMAT
            Use sprintf FORMAT for output file suffixes.

        --json, --csv, --table
            Output structured operation records in JSON, CSV, or tabular format.

        --pipe COMMAND
            Send output directly through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        csplit file.txt /CHAPTER/ {*}
            Split file at each occurrence of CHAPTER.

        csplit -f section_ -n 3 server.log 100 200
            Split at line numbers 100 and 200 with 3-digit suffixes.

        csplit --json data.txt /SECTION/ {*}
            Split data and emit JSON status records.

    CrossShell for UNIX                                                    csplit(1)
)";
    }

    static void printVersion() {
        std::cout << "csplit 1.0\n";
    }

    static bool parsePattern(const std::string& arg, SplitPattern& pat) {
        if (arg.empty()) return false;

        if (arg[0] == '/' || arg[0] == '%') {
            char delim = arg[0];
            pat.type = (delim == '/') ? PatternType::RegexMatch : PatternType::RegexSkip;
            size_t endDelim = arg.find(delim, 1);
            if (endDelim == std::string::npos) return false;

            pat.regexStr = arg.substr(1, endDelim - 1);
            if (endDelim + 1 < arg.size()) {
                pat.offset = std::stoll(arg.substr(endDelim + 1));
            }
            return true;
        } else if (arg[0] == '{') {
            if (arg.back() != '}') return false;
            std::string rep = arg.substr(1, arg.size() - 2);
            if (rep == "*") pat.repeat = -1;
            else pat.repeat = std::stoi(rep);
            return true;
        } else if (std::isdigit(static_cast<unsigned char>(arg[0]))) {
            pat.type = PatternType::LineNum;
            pat.lineNum = std::stoll(arg);
            return true;
        }
        return false;
    }

    static bool parse(int argc, char* argv[], CsplitOptions& opts) {
        int i = 1;
        for (; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h" || arg == "/?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-k" || arg == "--keep-files") {
                opts.keepFiles = true;
            } else if (arg == "-s" || arg == "--quiet" || arg == "--silent") {
                opts.quiet = true;
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if ((arg == "-f" || arg == "--prefix") && i + 1 < argc) {
                opts.prefix = argv[++i];
            } else if ((arg == "-n" || arg == "--digits") && i + 1 < argc) {
                opts.digits = std::stoi(argv[++i]);
            } else if ((arg == "-b" || arg == "--suffix-format") && i + 1 < argc) {
                opts.suffixFormat = argv[++i];
            } else if (arg == "--") {
                ++i;
                break;
            } else if (arg[0] == '-') {
                std::cerr << "csplit: unknown option '" << arg << "'\n";
                return false;
            } else {
                break;
            }
        }

        if (i >= argc) {
            std::cerr << "csplit: missing file operand\n";
            return false;
        }
        opts.inputFile = argv[i++];

        for (; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg[0] == '{' && !opts.patterns.empty()) {
                SplitPattern repeatPat;
                if (parsePattern(arg, repeatPat)) {
                    opts.patterns.back().repeat = repeatPat.repeat;
                }
            } else {
                SplitPattern pat;
                if (parsePattern(arg, pat)) {
                    opts.patterns.push_back(pat);
                } else {
                    std::cerr << "csplit: invalid pattern '" << arg << "'\n";
                    return false;
                }
            }
        }

        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class CsplitReporter {
public:
    static int dispatch(const std::vector<std::pair<std::string, size_t>>& pieces,
                        int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"pieces\":[";
            for (size_t i = 0; i < pieces.size(); ++i) {
                if (i > 0) text += ",";
                text += "{\"file\":\"" + pieces[i].first + "\",\"bytes\":" + std::to_string(pieces[i].second) + "}";
            }
            text += "]}\n";
        } else if (format == 2) {
            text = "file,bytes\n";
            for (const auto& p : pieces) {
                text += "\"" + p.first + "\"," + std::to_string(p.second) + "\n";
            }
        } else if (format == 3) {
            text = "FILE\tBYTES\n--------------------\n";
            for (const auto& p : pieces) {
                text += p.first + "\t" + std::to_string(p.second) + "\n";
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
// 3. CONTEXT SPLIT ENGINE
// ============================================================================

class FileSplitManager {
public:
    static std::string formatFilename(const std::string& prefix, int index, int digits, const std::string& customFmt) {
        if (!customFmt.empty()) {
            char buf[256];
            snprintf(buf, sizeof(buf), customFmt.c_str(), index);
            return prefix + buf;
        } else {
            std::ostringstream ss;
            ss << prefix << std::setfill('0') << std::setw(digits) << index;
            return ss.str();
        }
    }

    static void cleanup(const std::vector<std::string>& files) {
        for (const auto& f : files) {
            std::remove(f.c_str());
        }
    }
};

class CsplitEngine {
private:
    CsplitOptions options;

public:
    explicit CsplitEngine(CsplitOptions opts) : options(std::move(opts)) {}

    int execute() {
        std::vector<std::string> lines;
        if (options.inputFile == "-") {
            std::string line;
            while (std::getline(std::cin, line)) {
                lines.push_back(line);
            }
        } else {
            std::ifstream in(options.inputFile);
            if (!in.is_open()) {
                std::cerr << "csplit: cannot open '" << options.inputFile << "'\n";
                return 1;
            }
            std::string line;
            while (std::getline(in, line)) {
                lines.push_back(line);
            }
        }

        std::vector<std::string> createdFiles;
        std::vector<std::pair<std::string, size_t>> pieces;
        int fileIndex = 0;
        size_t currentLine = 0;

        auto writePiece = [&](size_t start, size_t end, bool skip) -> bool {
            if (skip) return true;
            std::string outName = FileSplitManager::formatFilename(options.prefix, fileIndex++, options.digits, options.suffixFormat);
            std::ofstream out(outName, std::ios::binary);
            if (!out.is_open()) return false;

            size_t totalBytes = 0;
            for (size_t i = start; i < end && i < lines.size(); ++i) {
                out << lines[i] << "\n";
                totalBytes += lines[i].size() + 1;
            }
            out.close();

            createdFiles.push_back(outName);
            pieces.emplace_back(outName, totalBytes);
            if (!options.quiet && options.outputFormat == 0 && options.pipeCommand.empty()) {
                std::cout << totalBytes << "\n";
            }
            return true;
        };

        for (const auto& pat : options.patterns) {
            int iterations = (pat.repeat == -1) ? INT_MAX : (pat.repeat + 1);

            for (int rep = 0; rep < iterations && currentLine < lines.size(); ++rep) {
                size_t splitAt = lines.size();

                if (pat.type == PatternType::LineNum) {
                    if (pat.lineNum > 0 && static_cast<size_t>(pat.lineNum - 1) > currentLine) {
                        splitAt = static_cast<size_t>(pat.lineNum - 1);
                    } else {
                        break;
                    }
                } else {
                    std::regex re(pat.regexStr);
                    for (size_t i = currentLine; i < lines.size(); ++i) {
                        if (std::regex_search(lines[i], re)) {
                            long long target = static_cast<long long>(i) + pat.offset;
                            if (target >= static_cast<long long>(currentLine) && target <= static_cast<long long>(lines.size())) {
                                splitAt = static_cast<size_t>(target);
                                break;
                            }
                        }
                    }
                    if (splitAt == lines.size() && rep > 0) {
                        break; // {N} / {*} finished
                    }
                }

                if (splitAt > currentLine) {
                    if (!writePiece(currentLine, splitAt, pat.type == PatternType::RegexSkip)) {
                        if (!options.keepFiles) FileSplitManager::cleanup(createdFiles);
                        return 1;
                    }
                    currentLine = splitAt;
                }
            }
        }

        if (currentLine < lines.size()) {
            if (!writePiece(currentLine, lines.size(), false)) {
                if (!options.keepFiles) FileSplitManager::cleanup(createdFiles);
                return 1;
            }
        }

        if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
            CsplitReporter::dispatch(pieces, options.outputFormat, options.pipeCommand);
        }

        return 0;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class CsplitApp {
public:
    static int run(int argc, char* argv[]) {
        CsplitOptions options;
        if (!CsplitOptions::parse(argc, argv, options)) {
            return 1;
        }
        CsplitEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return CsplitApp::run(argc, argv);
}
