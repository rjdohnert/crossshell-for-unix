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
 * SINGLE FILE INDEX: split.cpp
 * ============================================================================
 * WinSplit - Object-Oriented File and Stream Chunk Splitter for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & SUFFIX GENERATOR] .......... SplitOptions and SuffixGenerator classes
 * 2. [STRUCTURED OUTPUT REPORTER] .......... OutputReporter class (JSON/CSV/Table/Pipe)
 * 3. [CORE SPLIT ENGINES] .................. SplitEngine class (lines, bytes, chunks)
 * 4. [APPLICATION CONTROLLER] .............. SplitApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <io.h>
#include <memory>

// ============================================================================
// 1. OPTIONS & SUFFIX GENERATOR
// ============================================================================

enum class SuffixType { Alpha, Numeric, Hex };
enum class SplitMode { Lines, Bytes, Chunks };

class SuffixGenerator {
public:
    static std::string generate(uint64_t index, int len, SuffixType type) {
        std::string s(len, ' ');
        uint64_t base = (type == SuffixType::Alpha) ? 26 : ((type == SuffixType::Numeric) ? 10 : 16);

        for (int i = len - 1; i >= 0; --i) {
            uint64_t rem = index % base;
            if (type == SuffixType::Alpha) {
                s[i] = static_cast<char>('a' + rem);
            } else if (type == SuffixType::Numeric) {
                s[i] = static_cast<char>('0' + rem);
            } else {
                s[i] = (rem < 10) ? static_cast<char>('0' + rem) : static_cast<char>('a' + (rem - 10));
            }
            index /= base;
        }
        return s;
    }
};

class SplitOptions {
public:
    SplitMode mode{SplitMode::Lines};
    uint64_t lineCount{1000};
    uint64_t byteCount{0};
    uint64_t chunkCount{0};

    int suffixLen{2};
    SuffixType suffixType{SuffixType::Alpha};

    std::string inputFile{"-"};
    std::string prefix{"x"};
    std::string filterCommand;
    int outputFormat{0}; // 0: raw, 1: JSON, 2: CSV, 3: Table
    std::string pipeCommand;

    static void printUsage(const char* progName) {
        std::cout << R"(split(1)                CrossShell for UNIX Reference Manual                 split(1)

    NAME
        split - split a file into pieces

    SYNOPSIS
        split [OPTIONS] [INPUT [PREFIX]]

    DESCRIPTION
        Output pieces of INPUT to PREFIXaa, PREFIXab, ...; default SIZE is 1000
        lines, and default PREFIX is 'x'. Reads standard input when INPUT is '-'
        or omitted. Multiplier suffixes (b=512, k=1024, m=1048576, g=1073741824,
        t=1099511627776) are supported for byte counts.

    OPTIONS
        -a, --suffix-length=N
            Use suffixes of length N (default: 2).

        -b, --bytes=SIZE
            Put SIZE bytes per output file.

        -l, --lines=NUMBER
            Put NUMBER lines per output file.

        -n, --number=CHUNKS
            Split into CHUNKS equal output files.

        -d, --numeric-suffixes
            Use numeric suffixes instead of alphabetic.

        -x
            Use hexadecimal suffixes instead of alphabetic.

        --json
            Emit split telemetry in JSON format.

        --csv
            Emit split telemetry in CSV format.

        --table
            Emit split telemetry in tabular format.

        --pipe COMMAND
            Stream status directly into COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Display version information and exit.

    EXAMPLES
        split -l 500 largefile.txt chunk_
            Split largefile.txt into 500-line chunks prefixed with chunk_.

        split -b 10m archive.tar part_
            Split archive into 10 MB pieces.

        split -d -a 3 -b 1k data.bin slice_
            Split data into 1 KB files with 3-digit numeric suffixes.

    CrossShell for UNIX                                                    split(1)
    )";
    }

    static uint64_t parseSize(const std::string& str) {
        if (str.empty()) return 0;
        uint64_t mult = 1;
        std::string numStr = str;

        char lastChar = static_cast<char>(std::tolower(static_cast<unsigned char>(str.back())));
        if (std::isalpha(static_cast<unsigned char>(lastChar))) {
            numStr = str.substr(0, str.length() - 1);
            switch (lastChar) {
                case 'b': mult = 512; break;
                case 'k': mult = 1024; break;
                case 'm': mult = 1024ULL * 1024ULL; break;
                case 'g': mult = 1024ULL * 1024ULL * 1024ULL; break;
                case 't': mult = 1024ULL * 1024ULL * 1024ULL * 1024ULL; break;
                default: mult = 1; break;
            }
        }
        return std::stoull(numStr) * mult;
    }

    static bool parse(int argc, char* argv[], SplitOptions& opts) {
        std::vector<std::string> positional;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-h" || arg == "/?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg.rfind("-a", 0) == 0 && arg.length() > 2) {
                opts.suffixLen = std::stoi(arg.substr(2));
            } else if (arg == "-a" || arg == "--suffix-length") {
                if (i + 1 < argc) opts.suffixLen = std::stoi(argv[++i]);
            } else if (arg.rfind("-b", 0) == 0 && arg.length() > 2) {
                opts.mode = SplitMode::Bytes;
                opts.byteCount = parseSize(arg.substr(2));
            } else if (arg == "-b" || arg == "--bytes") {
                if (i + 1 < argc) {
                    opts.mode = SplitMode::Bytes;
                    opts.byteCount = parseSize(argv[++i]);
                }
            } else if (arg.rfind("-l", 0) == 0 && arg.length() > 2) {
                opts.mode = SplitMode::Lines;
                opts.lineCount = parseSize(arg.substr(2));
            } else if (arg == "-l" || arg == "--lines") {
                if (i + 1 < argc) {
                    opts.mode = SplitMode::Lines;
                    opts.lineCount = parseSize(argv[++i]);
                }
            } else if (arg.rfind("-n", 0) == 0 && arg.length() > 2) {
                opts.mode = SplitMode::Chunks;
                opts.chunkCount = parseSize(arg.substr(2));
            } else if (arg == "-n" || arg == "--number") {
                if (i + 1 < argc) {
                    opts.mode = SplitMode::Chunks;
                    opts.chunkCount = parseSize(argv[++i]);
                }
            } else if (arg == "-d" || arg == "--numeric-suffixes") {
                opts.suffixType = SuffixType::Numeric;
            } else if (arg == "-x") {
                opts.suffixType = SuffixType::Hex;
            } else if (arg == "-") {
                positional.push_back(arg);
            } else if (!arg.empty() && arg[0] == '-') {
                std::cerr << "split: unrecognized option '" << arg << "'\n";
                return false;
            } else {
                positional.push_back(arg);
            }
        }

        if (!positional.empty()) opts.inputFile = positional[0];
        if (positional.size() > 1) opts.prefix = positional[1];

        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class OutputReporter {
private:
    int format;
    std::string pipeCommand;

public:
    OutputReporter(int fmt, std::string pipeCmd)
        : format(fmt), pipeCommand(std::move(pipeCmd)) {}

    int finish(const std::string& prefix, uint64_t files) const {
        if (format == 0 && pipeCommand.empty()) return 0;

        std::string text;
        if (format == 1) {
            text = "{\"status\":\"success\",\"prefix\":\"" + prefix + "\",\"files\":" + std::to_string(files) + "}\n";
        } else if (format == 2) {
            text = "status,prefix,files\nsuccess," + prefix + "," + std::to_string(files) + "\n";
        } else if (format == 3) {
            text = "STATUS\tPREFIX\tFILES\nsuccess\t" + prefix + "\t" + std::to_string(files) + "\n";
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
// 3. CORE SPLIT ENGINES
// ============================================================================

class SplitEngine {
private:
    SplitOptions options;
    OutputReporter reporter;

public:
    explicit SplitEngine(SplitOptions opts)
        : options(opts), reporter(opts.outputFormat, opts.pipeCommand) {}

    int execute() {
        _setmode(_fileno(stdin), _O_BINARY);

        std::istream* inputStream = &std::cin;
        std::ifstream fileIn;

        if (options.inputFile != "-") {
            fileIn.open(options.inputFile, std::ios::binary);
            if (!fileIn.is_open()) {
                std::cerr << "split: cannot open '" << options.inputFile << "' for reading\n";
                return 1;
            }
            inputStream = &fileIn;
        }

        uint64_t filesCreated = 0;

        if (options.mode == SplitMode::Lines) {
            filesCreated = splitByLines(*inputStream);
        } else if (options.mode == SplitMode::Bytes) {
            filesCreated = splitByBytes(*inputStream);
        } else if (options.mode == SplitMode::Chunks) {
            filesCreated = splitByChunks(*inputStream);
        }

        return reporter.finish(options.prefix, filesCreated);
    }

private:
    uint64_t splitByLines(std::istream& in) {
        uint64_t fileIdx = 0;
        std::string line;
        std::ofstream currentOut;
        uint64_t linesInCurrent = 0;

        while (std::getline(in, line)) {
            if (!currentOut.is_open() || linesInCurrent >= options.lineCount) {
                if (currentOut.is_open()) currentOut.close();
                std::string fname = options.prefix + SuffixGenerator::generate(fileIdx++, options.suffixLen, options.suffixType);
                currentOut.open(fname, std::ios::binary);
                linesInCurrent = 0;
            }

            currentOut << line << "\n";
            linesInCurrent++;
        }

        if (currentOut.is_open()) currentOut.close();
        return fileIdx;
    }

    uint64_t splitByBytes(std::istream& in) {
        uint64_t fileIdx = 0;
        constexpr size_t BUF_SIZE = 65536;
        std::vector<char> buffer(BUF_SIZE);
        std::ofstream currentOut;
        uint64_t bytesInCurrent = 0;

        while (in) {
            uint64_t remaining = options.byteCount - bytesInCurrent;
            size_t toRead = static_cast<size_t>(std::min(static_cast<uint64_t>(BUF_SIZE), remaining));
            in.read(buffer.data(), toRead);
            size_t bytesRead = static_cast<size_t>(in.gcount());

            if (bytesRead == 0) break;

            if (!currentOut.is_open()) {
                std::string fname = options.prefix + SuffixGenerator::generate(fileIdx++, options.suffixLen, options.suffixType);
                currentOut.open(fname, std::ios::binary);
                bytesInCurrent = 0;
            }

            currentOut.write(buffer.data(), bytesRead);
            bytesInCurrent += bytesRead;

            if (bytesInCurrent >= options.byteCount) {
                currentOut.close();
                bytesInCurrent = 0;
            }
        }

        if (currentOut.is_open()) currentOut.close();
        return fileIdx;
    }

    uint64_t splitByChunks(std::istream& in) {
        if (options.chunkCount == 0) return 0;

        std::vector<char> fullData((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        size_t totalBytes = fullData.size();
        size_t chunkSize = (totalBytes + options.chunkCount - 1) / options.chunkCount;

        uint64_t fileIdx = 0;
        size_t offset = 0;

        while (offset < totalBytes) {
            size_t curChunk = std::min(chunkSize, totalBytes - offset);
            std::string fname = options.prefix + SuffixGenerator::generate(fileIdx++, options.suffixLen, options.suffixType);
            std::ofstream out(fname, std::ios::binary);
            out.write(fullData.data() + offset, curChunk);
            out.close();
            offset += curChunk;
        }

        return fileIdx;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class SplitApp {
public:
    static int run(int argc, char* argv[]) {
        SplitOptions options;
        if (!SplitOptions::parse(argc, argv, options)) {
            return 1;
        }

        SplitEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return SplitApp::run(argc, argv);
}
