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
 * SINGLE FILE INDEX: uniq.cpp
 * ============================================================================
 * WinUniq - Object-Oriented Duplicate Line Filter & Stream Deduplicator
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [CONFIGURATION ENUMS & OPTIONS] ....... UniqOptions class, CLI parser and help
 * 2. [LINE EXTRACTION & COMPARATOR] ........ LineExtractor and LineComparator classes
 * 3. [STREAM & PIPE I/O MANAGEMENT] ........ LineReader and StreamManager classes
 * 4. [CORE DEDUPLICATION ENGINE] ........... LineGroup and UniqEngine classes
 * 5. [APPLICATION CONTROLLER] .............. UniqApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <algorithm>
#include <iomanip>
#include <cstdint>
#include <sstream>
#include <cctype>
#include <optional>

// ============================================================================
// 1. CONFIGURATION ENUMS & OPTIONS
// ============================================================================

enum class AllRepeatedMode {
    None,       // Default / inactive
    Separate,   // Separate groups with an empty line
    Prepend     // Prepend an empty line before each group
};

enum class GroupMode {
    None,
    Separate,   // Separate groups with an empty line (default)
    Prepend,    // Prepend empty line before every group
    Append,     // Append empty line after every group
    Both        // Prepend and append empty line
};

class UniqOptions {
public:
    bool count{false};
    bool repeatedOnly{false};
    bool uniqueOnly{false};
    bool ignoreCase{false};
    bool zeroTerminated{false};

    size_t skipFields{0};
    size_t skipChars{0};
    size_t checkChars{0}; // 0 = unlimited

    AllRepeatedMode allRepeated{AllRepeatedMode::None};
    GroupMode groupMode{GroupMode::None};

    std::string inputPath;
    std::string outputPath;

    static void showHelp() {
        std::cout << R"(uniq(1)                 CrossShell for UNIX Reference Manual                 uniq(1)

    NAME
        uniq - report or omit repeated lines

    SYNOPSIS
        uniq [OPTIONS] [INPUT [OUTPUT]]

    DESCRIPTION
        Filter adjacent matching lines from INPUT (or standard input), writing
        to OUTPUT (or standard output).
        With no options, matching lines are merged to the first occurrence.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -c, --count
            Prefix lines by the number of occurrences.

        -d, --repeated
            Only print duplicate lines, one for each group.

        -D
            Print all duplicate lines.

        --all-repeated[=METHOD]
            Like -D, but allow custom separation: none, prepend, separate.

        -f, --skip-fields=N
            Avoid comparing the first N whitespace-separated fields.

        -i, --ignore-case
            Ignore differences in case when comparing.

        -s, --skip-chars=N
            Avoid comparing the first N characters.

        -u, --unique
            Only print unique lines (occurring exactly once).

        -w, --check-chars=N
            Compare at most N characters in lines.

        -z, --zero-terminated
            Line delimiter is NUL (\0), not newline.

        --group[=METHOD]
            Separate groups using METHOD: separate, prepend, append, both.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        sequence log.txt | uniq -c
            Count duplicate occurrences in a sorted log.

        uniq -d input.txt duplicates.txt
            Write only duplicate lines to duplicates.txt.

        uniq -i -f 2 data.csv
            Case-insensitive deduplication, ignoring first 2 fields.

    CrossShell for UNIX                                                      uniq(1)
)";
    }

    static void showVersion() {
        std::cout << "uniq 8.1\n";
        std::cout << "Copyright (C) 2026, Roberto J Dohnert.\n";
    }

    static UniqOptions parse(int argc, char* argv[]) {
        UniqOptions opts;
        std::vector<std::string> positional;

        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") {
                showHelp();
                std::exit(0);
            } else if (arg == "--version" || arg == "-v" || arg == "-V") {
                showVersion();
                std::exit(0);
            } else if (arg == "-c" || arg == "--count") {
                opts.count = true;
            } else if (arg == "-d" || arg == "--repeated") {
                opts.repeatedOnly = true;
            } else if (arg == "-u" || arg == "--unique") {
                opts.uniqueOnly = true;
            } else if (arg == "-i" || arg == "--ignore-case") {
                opts.ignoreCase = true;
            } else if (arg == "-z" || arg == "--zero-terminated") {
                opts.zeroTerminated = true;
            } else if (arg == "-D") {
                opts.allRepeated = AllRepeatedMode::None;
                opts.repeatedOnly = true;
            } else if (arg.rfind("--all-repeated", 0) == 0) {
                opts.repeatedOnly = true;
                size_t eqPos = arg.find('=');
                if (eqPos == std::string_view::npos || arg.substr(eqPos + 1) == "none") {
                    opts.allRepeated = AllRepeatedMode::None;
                } else if (arg.substr(eqPos + 1) == "prepend") {
                    opts.allRepeated = AllRepeatedMode::Prepend;
                } else if (arg.substr(eqPos + 1) == "separate") {
                    opts.allRepeated = AllRepeatedMode::Separate;
                }
            } else if (arg.rfind("--group", 0) == 0) {
                size_t eqPos = arg.find('=');
                if (eqPos == std::string_view::npos || arg.substr(eqPos + 1) == "separate") {
                    opts.groupMode = GroupMode::Separate;
                } else if (arg.substr(eqPos + 1) == "prepend") {
                    opts.groupMode = GroupMode::Prepend;
                } else if (arg.substr(eqPos + 1) == "append") {
                    opts.groupMode = GroupMode::Append;
                } else if (arg.substr(eqPos + 1) == "both") {
                    opts.groupMode = GroupMode::Both;
                }
            } else if (arg.rfind("-f", 0) == 0 && arg.length() > 2) {
                opts.skipFields = std::stoul(std::string(arg.substr(2)));
            } else if (arg == "-f" || arg == "--skip-fields") {
                if (i + 1 < argc) opts.skipFields = std::stoul(argv[++i]);
            } else if (arg.rfind("-s", 0) == 0 && arg.length() > 2) {
                opts.skipChars = std::stoul(std::string(arg.substr(2)));
            } else if (arg == "-s" || arg == "--skip-chars") {
                if (i + 1 < argc) opts.skipChars = std::stoul(argv[++i]);
            } else if (arg.rfind("-w", 0) == 0 && arg.length() > 2) {
                opts.checkChars = std::stoul(std::string(arg.substr(2)));
            } else if (arg == "-w" || arg == "--check-chars") {
                if (i + 1 < argc) opts.checkChars = std::stoul(argv[++i]);
            } else if (arg.length() > 1 && arg[0] == '-' && arg[1] != '-') {
                for (size_t j = 1; j < arg.length(); ++j) {
                    switch (arg[j]) {
                        case 'c': opts.count = true; break;
                        case 'd': opts.repeatedOnly = true; break;
                        case 'u': opts.uniqueOnly = true; break;
                        case 'i': opts.ignoreCase = true; break;
                        case 'z': opts.zeroTerminated = true; break;
                        case 'D': opts.allRepeated = AllRepeatedMode::None; opts.repeatedOnly = true; break;
                        default:
                            std::cerr << "uniq: invalid option -- '" << arg[j] << "'\n";
                            std::cerr << "Try 'uniq --help' for more information.\n";
                            std::exit(1);
                    }
                }
            } else {
                positional.push_back(std::string(arg));
            }
        }

        if (!positional.empty()) opts.inputPath = positional[0];
        if (positional.size() > 1) opts.outputPath = positional[1];

        return opts;
    }
};

// ============================================================================
// 2. LINE EXTRACTION & COMPARATOR
// ============================================================================

class LineExtractor {
public:
    static std::string_view extractKey(std::string_view line, size_t skipFields, size_t skipChars, size_t checkChars) {
        size_t offset = 0;

        for (size_t f = 0; f < skipFields && offset < line.length(); ++f) {
            while (offset < line.length() && std::isspace(static_cast<unsigned char>(line[offset]))) {
                offset++;
            }
            while (offset < line.length() && !std::isspace(static_cast<unsigned char>(line[offset]))) {
                offset++;
            }
        }

        offset = (std::min)(line.length(), offset + skipChars);
        std::string_view key = line.substr(offset);

        if (checkChars > 0 && checkChars < key.length()) {
            key = key.substr(0, checkChars);
        }

        return key;
    }
};

class LineComparator {
private:
    bool ignoreCase;
    size_t skipFields;
    size_t skipChars;
    size_t checkChars;

public:
    LineComparator(bool iCase, size_t fields, size_t chars, size_t maxChars)
        : ignoreCase(iCase), skipFields(fields), skipChars(chars), checkChars(maxChars) {}

    [[nodiscard]] bool areEqual(std::string_view lineA, std::string_view lineB) const {
        std::string_view keyA = LineExtractor::extractKey(lineA, skipFields, skipChars, checkChars);
        std::string_view keyB = LineExtractor::extractKey(lineB, skipFields, skipChars, checkChars);

        if (keyA.length() != keyB.length()) return false;

        if (ignoreCase) {
            return std::equal(keyA.begin(), keyA.end(), keyB.begin(), [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            });
        }

        return keyA == keyB;
    }
};

// ============================================================================
// 3. STREAM & PIPE I/O MANAGEMENT
// ============================================================================

class LineReader {
private:
    std::istream& stream;
    char delimiter;

public:
    LineReader(std::istream& in, bool zeroTerminated)
        : stream(in), delimiter(zeroTerminated ? '\0' : '\n') {}

    bool readNextLine(std::string& line) {
        if (!std::getline(stream, line, delimiter)) {
            return false;
        }
        if (delimiter == '\n' && !line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        return true;
    }
};

class StreamManager {
public:
    static void configurePipes() {
        #ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
        #endif
    }
};

// ============================================================================
// 4. CORE DEDUPLICATION ENGINE
// ============================================================================

struct LineGroup {
    std::string representativeLine;
    std::vector<std::string> allLines;
    uint64_t count{0};

    void reset(std::string firstLine) {
        representativeLine = firstLine;
        allLines.clear();
        allLines.push_back(std::move(firstLine));
        count = 1;
    }

    void add(std::string line) {
        allLines.push_back(std::move(line));
        count++;
    }
};

class UniqEngine {
private:
    UniqOptions options;
    LineComparator comparator;
    char delimiter;
    bool hasEmittedFirstGroup{false};

public:
    explicit UniqEngine(UniqOptions opts)
        : options(std::move(opts)),
          comparator(options.ignoreCase, options.skipFields, options.skipChars, options.checkChars),
          delimiter(options.zeroTerminated ? '\0' : '\n') {}

    int execute() {
        StreamManager::configurePipes();

        std::unique_ptr<std::ifstream> fileIn;
        std::istream* inStream = &std::cin;

        if (!options.inputPath.empty() && options.inputPath != "-") {
            fileIn = std::make_unique<std::ifstream>(options.inputPath, std::ios::binary);
            if (!fileIn->is_open()) {
                std::cerr << "uniq: cannot open '" << options.inputPath << "': No such file\n";
                return 1;
            }
            inStream = fileIn.get();
        }

        std::unique_ptr<std::ofstream> fileOut;
        std::ostream* outStream = &std::cout;

        if (!options.outputPath.empty() && options.outputPath != "-") {
            fileOut = std::make_unique<std::ofstream>(options.outputPath, std::ios::binary);
            if (!fileOut->is_open()) {
                std::cerr << "uniq: cannot create '" << options.outputPath << "': Permission denied\n";
                return 1;
            }
            outStream = fileOut.get();
        }

        LineReader reader(*inStream, options.zeroTerminated);
        std::string currentLine;

        if (!reader.readNextLine(currentLine)) {
            return 0; // Empty input
        }

        LineGroup currentGroup;
        currentGroup.reset(std::move(currentLine));

        while (reader.readNextLine(currentLine)) {
            if (comparator.areEqual(currentGroup.representativeLine, currentLine)) {
                currentGroup.add(std::move(currentLine));
            } else {
                emitGroup(currentGroup, *outStream);
                currentGroup.reset(std::move(currentLine));
            }
        }

        emitGroup(currentGroup, *outStream);
        outStream->flush();

        return 0;
    }

private:
    void emitGroup(const LineGroup& group, std::ostream& out) {
        if (options.groupMode != GroupMode::None) {
            if (!hasEmittedFirstGroup) {
                if (options.groupMode == GroupMode::Prepend || options.groupMode == GroupMode::Both) {
                    out << delimiter;
                }
            } else {
                if (options.groupMode == GroupMode::Separate || options.groupMode == GroupMode::Both) {
                    out << delimiter;
                }
            }
        }

        hasEmittedFirstGroup = true;

        if (options.allRepeated != AllRepeatedMode::None || (options.repeatedOnly && group.allLines.size() > 1 && !options.count && !options.uniqueOnly && options.allRepeated != AllRepeatedMode::None)) {
            if (group.count > 1) {
                if (options.allRepeated == AllRepeatedMode::Prepend || options.allRepeated == AllRepeatedMode::Separate) {
                    out << delimiter;
                }
                for (const auto& line : group.allLines) {
                    out << line << delimiter;
                }
            }
            return;
        }

        if (options.uniqueOnly && group.count > 1) return;
        if (options.repeatedOnly && group.count < 2) return;

        if (options.count) {
            out << std::setw(7) << group.count << " " << group.representativeLine << delimiter;
        } else {
            out << group.representativeLine << delimiter;
        }

        if (options.groupMode == GroupMode::Append || options.groupMode == GroupMode::Both) {
            out << delimiter;
        }
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class UniqApp {
public:
    static int run(int argc, char* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        try {
            UniqOptions options = UniqOptions::parse(argc, argv);
            UniqEngine engine(std::move(options));
            return engine.execute();
        } catch (const std::exception& ex) {
            std::cerr << "uniq: fatal error: " << ex.what() << "\n";
            return 1;
        }
    }
};

int main(int argc, char* argv[]) {
    return UniqApp::run(argc, argv);
}