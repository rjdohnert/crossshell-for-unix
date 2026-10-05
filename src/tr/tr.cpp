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
 * SINGLE FILE INDEX: tr.cpp
 * ============================================================================
 * WinTr - Object-Oriented Character Translation & Squeeze Engine for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS PARSER & HELP ENGINE] ........ TrOptions class (CLI flags, formats, pipes)
 * 2. [PATTERN & RANGE EXPANDER] ............ PatternExpander class (classes, ranges, repeats)
 * 3. [STRUCTURED OUTPUT FORMATTER] ......... OutputFormatter class (raw/JSON/CSV/table/pipe)
 * 4. [CORE TRANSLATION ENGINE] ............. TrEngine class (delete, squeeze, translate)
 * 5. [APPLICATION CONTROLLER] .............. TrApp class and main entry point
 * ============================================================================
 */

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <cctype>
#include <cstdlib>
#include <algorithm>
#include <memory>
#include <io.h>
#include <fcntl.h>

// ============================================================================
// 1. OPTIONS PARSER & HELP ENGINE
// ============================================================================

class TrOptions {
public:
    bool complement{false};    // -c / -C
    bool deleteChars{false};   // -d
    bool squeeze{false};       // -s
    int outputFormat{0};       // 0: raw, 1: JSON, 2: CSV, 3: Table
    std::string pipeCommand;
    std::vector<std::string> args;

    static void printHelp() {
        std::cout << R"(tr(1)                   CrossShell for UNIX Reference Manual                 tr(1)

    NAME
        tr - translate or delete characters

    SYNOPSIS
        tr [OPTIONS] STRING1 [STRING2]

    DESCRIPTION
        Translate, squeeze, and/or delete characters from standard input,
        writing to standard output.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -c, -C, --complement
            Use the complement of STRING1.

        -d, --delete
            Delete characters in STRING1, do not translate.

        -s, --squeeze-repeats
            Replace each sequence of a repeated character that is listed in the
            last specified STRING with a single occurrence of that character.

        --json, --csv, --table
            Format transformed output records.

        --pipe COMMAND
            Send formatted output through COMMAND.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        tr a-z A-Z
            Translate lowercase characters to uppercase.

        tr -d 0-9
            Delete all digits from input.

        tr -s " "
            Squeeze consecutive spaces into single spaces.

    CrossShell for UNIX                                                      tr(1)
)";
    }

    static bool parse(int argc, char* argv[], TrOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--json") { opts.outputFormat = 1; continue; }
            if (arg == "--csv") { opts.outputFormat = 2; continue; }
            if (arg == "--table") { opts.outputFormat = 3; continue; }
            if (arg == "--pipe" && i + 1 < argc) { opts.pipeCommand = argv[++i]; continue; }
            if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") {
                printHelp();
                std::exit(0);
            }
            if (arg == "-v" || arg == "--version") {
                std::cout << "tr version 1.0.0\n";
                std::exit(0);
            } else if (arg == "-c" || arg == "-C" || arg == "--complement") {
                opts.complement = true;
            } else if (arg == "-d" || arg == "--delete") {
                opts.deleteChars = true;
            } else if (arg == "-s" || arg == "--squeeze-repeats") {
                opts.squeeze = true;
            } else if (arg.rfind("-", 0) == 0 && arg.length() > 1 && arg[1] != '-') {
                for (size_t j = 1; j < arg.length(); ++j) {
                    char ch = arg[j];
                    if (ch == 'c' || ch == 'C') opts.complement = true;
                    else if (ch == 'd' || ch == 'D') opts.deleteChars = true;
                    else if (ch == 's') opts.squeeze = true;
                    else {
                        std::cerr << "tr: invalid option -- '" << ch << "'\n";
                        return false;
                    }
                }
            } else {
                opts.args.push_back(arg);
            }
        }

        if (opts.args.empty()) {
            std::cerr << "tr: missing operand\nTry 'tr --help' for more information.\n";
            return false;
        }

        return true;
    }
};

// ============================================================================
// 2. PATTERN & RANGE EXPANDER
// ============================================================================

class PatternExpander {
public:
    static std::vector<uint8_t> expand(const std::string& str, size_t targetLen = 0) {
        std::vector<uint8_t> result;
        size_t i = 0;
        size_t len = str.length();

        while (i < len) {
            // POSIX Character Classes: [:class:]
            if (i + 2 < len && str[i] == '[' && str[i + 1] == ':') {
                size_t endCls = str.find(":]", i + 2);
                if (endCls != std::string::npos) {
                    std::string cls = str.substr(i + 2, endCls - (i + 2));
                    bool matched = true;

                    for (int c = 0; c < 256; ++c) {
                        unsigned char uc = static_cast<unsigned char>(c);
                        if (cls == "alnum"  && std::isalnum(uc))  result.push_back(uc);
                        else if (cls == "alpha"  && std::isalpha(uc))  result.push_back(uc);
                        else if (cls == "cntrl"  && std::iscntrl(uc))  result.push_back(uc);
                        else if (cls == "digit"  && std::isdigit(uc))  result.push_back(uc);
                        else if (cls == "graph"  && std::isgraph(uc))  result.push_back(uc);
                        else if (cls == "lower"  && std::islower(uc))  result.push_back(uc);
                        else if (cls == "print"  && std::isprint(uc))  result.push_back(uc);
                        else if (cls == "punct"  && std::ispunct(uc))  result.push_back(uc);
                        else if (cls == "space"  && std::isspace(uc))  result.push_back(uc);
                        else if (cls == "upper"  && std::isupper(uc))  result.push_back(uc);
                        else if (cls == "xdigit" && std::isxdigit(uc)) result.push_back(uc);
                        else if (c == 255 && cls != "alnum" && cls != "alpha" && cls != "cntrl" &&
                                 cls != "digit" && cls != "graph" && cls != "lower" &&
                                 cls != "print" && cls != "punct" && cls != "space" &&
                                 cls != "upper" && cls != "xdigit") {
                            matched = false;
                        }
                    }

                    if (matched) {
                        i = endCls + 2;
                        continue;
                    }
                }
            }

            // Character Repetition in string2: [c*n] or [c*]
            if (i + 2 < len && str[i] == '[' && str[i + 2] == '*') {
                size_t endRep = str.find(']', i + 3);
                if (endRep != std::string::npos) {
                    uint8_t repChar = static_cast<uint8_t>(str[i + 1]);
                    std::string countStr = str.substr(i + 3, endRep - (i + 3));
                    size_t count = 0;

                    if (countStr.empty()) {
                        if (targetLen > result.size()) {
                            count = targetLen - result.size();
                        }
                    } else {
                        if (countStr[0] == '0') {
                            count = std::strtoul(countStr.c_str(), nullptr, 8);
                        } else {
                            count = std::strtoul(countStr.c_str(), nullptr, 10);
                        }
                    }

                    for (size_t k = 0; k < count; ++k) {
                        result.push_back(repChar);
                    }
                    i = endRep + 1;
                    continue;
                }
            }

            // Parse character or escape sequence
            auto parseChar = [&](size_t& idx) -> uint8_t {
                if (str[idx] == '\\' && idx + 1 < len) {
                    idx++;
                    char esc = str[idx++];
                    switch (esc) {
                        case 'a': return '\a';
                        case 'b': return '\b';
                        case 'f': return '\f';
                        case 'n': return '\n';
                        case 'r': return '\r';
                        case 't': return '\t';
                        case 'v': return '\v';
                        case '\\': return '\\';
                        default:
                            if (esc >= '0' && esc <= '7') {
                                int oct = esc - '0';
                                int digits = 1;
                                while (idx < len && str[idx] >= '0' && str[idx] <= '7' && digits < 3) {
                                    oct = oct * 8 + (str[idx++] - '0');
                                    digits++;
                                }
                                return static_cast<uint8_t>(oct & 0xFF);
                            }
                            return static_cast<uint8_t>(esc);
                    }
                }
                return static_cast<uint8_t>(str[idx++]);
            };

            uint8_t c1 = parseChar(i);

            // Character Ranges: c1-c2
            if (i < len && str[i] == '-' && i + 1 < len) {
                i++;
                uint8_t c2 = parseChar(i);
                if (c1 <= c2) {
                    for (int c = c1; c <= c2; ++c) {
                        result.push_back(static_cast<uint8_t>(c));
                    }
                } else {
                    result.push_back(c1);
                    result.push_back(static_cast<uint8_t>('-'));
                    result.push_back(c2);
                }
            } else {
                result.push_back(c1);
            }
        }

        return result;
    }
};

// ============================================================================
// 3. STRUCTURED OUTPUT FORMATTER
// ============================================================================

class OutputFormatter {
public:
    static std::string jsonQuote(const std::string& value) {
        std::string out = "\"";
        for (unsigned char c : value) {
            if (c == '"' || c == '\\') out += '\\';
            if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else if (c == '\t') out += "\\t";
            else out += (c < 0x20) ? '?' : static_cast<char>(c);
        }
        return out + "\"";
    }

    static std::string csvQuote(const std::string& value) {
        std::string out = "\"";
        for (char c : value) {
            out += (c == '"') ? "\"\"" : std::string(1, c);
        }
        return out + "\"";
    }

    static int output(const std::string& content, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"output\":" + jsonQuote(content) + "}\n";
        } else if (format == 2) {
            text = "\"output\"\n" + csvQuote(content) + "\n";
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
            std::fwrite(text.data(), 1, text.size(), stdout);
        }
        return 0;
    }
};

// ============================================================================
// 4. CORE TRANSLATION ENGINE
// ============================================================================

class TrEngine {
private:
    TrOptions options;

public:
    explicit TrEngine(TrOptions opts) : options(std::move(opts)) {}

    int execute() {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        constexpr size_t BUFFER_SIZE = 65536;
        std::vector<uint8_t> inBuf(BUFFER_SIZE);
        std::vector<uint8_t> outBuf(BUFFER_SIZE);
        size_t outPos = 0;
        std::string structuredOutput;

        auto flushOut = [&]() {
            if (outPos > 0) {
                if (options.outputFormat == 0 && options.pipeCommand.empty()) {
                    std::fwrite(outBuf.data(), 1, outPos, stdout);
                } else {
                    structuredOutput.append(reinterpret_cast<const char*>(outBuf.data()), outPos);
                }
                outPos = 0;
            }
        };

        auto emitByte = [&](uint8_t b) {
            outBuf[outPos++] = b;
            if (outPos == BUFFER_SIZE) {
                flushOut();
            }
        };

        // MODE 1: DELETE ONLY (-d)
        if (options.deleteChars && !options.squeeze) {
            std::vector<uint8_t> v1 = PatternExpander::expand(options.args[0]);
            bool delSet[256] = { false };
            for (uint8_t b : v1) delSet[b] = true;

            if (options.complement) {
                for (int i = 0; i < 256; ++i) delSet[i] = !delSet[i];
            }

            size_t bytesRead = 0;
            while ((bytesRead = std::fread(inBuf.data(), 1, BUFFER_SIZE, stdin)) > 0) {
                for (size_t i = 0; i < bytesRead; ++i) {
                    uint8_t ch = inBuf[i];
                    if (!delSet[ch]) {
                        emitByte(ch);
                    }
                }
            }
            flushOut();
            if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
                return OutputFormatter::output(structuredOutput, options.outputFormat, options.pipeCommand);
            }
            return 0;
        }

        // MODE 2: DELETE AND SQUEEZE (-d -s)
        if (options.deleteChars && options.squeeze) {
            if (options.args.size() < 2) {
                std::cerr << "tr: missing operand after '" << options.args[0] << "' for -ds\n";
                return 1;
            }

            std::vector<uint8_t> v1 = PatternExpander::expand(options.args[0]);
            std::vector<uint8_t> v2 = PatternExpander::expand(options.args[1]);

            bool delSet[256] = { false };
            for (uint8_t b : v1) delSet[b] = true;
            if (options.complement) {
                for (int i = 0; i < 256; ++i) delSet[i] = !delSet[i];
            }

            bool sqzSet[256] = { false };
            for (uint8_t b : v2) sqzSet[b] = true;

            int lastCh = -1;
            size_t bytesRead = 0;
            while ((bytesRead = std::fread(inBuf.data(), 1, BUFFER_SIZE, stdin)) > 0) {
                for (size_t i = 0; i < bytesRead; ++i) {
                    uint8_t ch = inBuf[i];
                    if (!delSet[ch]) {
                        if (!sqzSet[ch] || ch != lastCh) {
                            emitByte(ch);
                            lastCh = ch;
                        }
                    }
                }
            }
            flushOut();
            if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
                return OutputFormatter::output(structuredOutput, options.outputFormat, options.pipeCommand);
            }
            return 0;
        }

        // MODE 3: SQUEEZE ONLY (-s)
        if (options.squeeze && options.args.size() == 1) {
            std::vector<uint8_t> v1 = PatternExpander::expand(options.args[0]);
            bool sqzSet[256] = { false };
            for (uint8_t b : v1) sqzSet[b] = true;
            if (options.complement) {
                for (int i = 0; i < 256; ++i) sqzSet[i] = !sqzSet[i];
            }

            int lastCh = -1;
            size_t bytesRead = 0;
            while ((bytesRead = std::fread(inBuf.data(), 1, BUFFER_SIZE, stdin)) > 0) {
                for (size_t i = 0; i < bytesRead; ++i) {
                    uint8_t ch = inBuf[i];
                    if (!sqzSet[ch] || ch != lastCh) {
                        emitByte(ch);
                        lastCh = ch;
                    }
                }
            }
            flushOut();
            if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
                return OutputFormatter::output(structuredOutput, options.outputFormat, options.pipeCommand);
            }
            return 0;
        }

        // MODE 4: TRANSLATION (STRING1 -> STRING2)
        if (options.args.size() < 2) {
            std::cerr << "tr: missing operand after '" << options.args[0] << "'\n";
            return 1;
        }

        std::vector<uint8_t> v1 = PatternExpander::expand(options.args[0]);
        std::vector<uint8_t> v2 = PatternExpander::expand(options.args[1], v1.size());

        if (v2.empty()) {
            std::cerr << "tr: string2 cannot be empty\n";
            return 1;
        }

        uint8_t mapTable[256];
        for (int i = 0; i < 256; ++i) mapTable[i] = static_cast<uint8_t>(i);

        bool sqzSet[256] = { false };
        if (options.squeeze) {
            for (uint8_t b : v2) sqzSet[b] = true;
        }

        if (options.complement) {
            bool inV1[256] = { false };
            for (uint8_t b : v1) inV1[b] = true;

            size_t v2Idx = 0;
            for (int i = 0; i < 256; ++i) {
                if (!inV1[i]) {
                    uint8_t target = (v2Idx < v2.size()) ? v2[v2Idx] : v2.back();
                    mapTable[i] = target;
                    v2Idx++;
                }
            }
        } else {
            for (size_t i = 0; i < v1.size(); ++i) {
                uint8_t src = v1[i];
                uint8_t dst = (i < v2.size()) ? v2[i] : v2.back();
                mapTable[src] = dst;
            }
        }

        int lastCh = -1;
        size_t bytesRead = 0;
        while ((bytesRead = std::fread(inBuf.data(), 1, BUFFER_SIZE, stdin)) > 0) {
            for (size_t i = 0; i < bytesRead; ++i) {
                uint8_t ch = mapTable[inBuf[i]];
                if (!options.squeeze || !sqzSet[ch] || ch != lastCh) {
                    emitByte(ch);
                    lastCh = ch;
                }
            }
        }
        flushOut();
        if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
            return OutputFormatter::output(structuredOutput, options.outputFormat, options.pipeCommand);
        }

        return 0;
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class TrApp {
public:
    static int run(int argc, char* argv[]) {
        TrOptions options;
        if (!TrOptions::parse(argc, argv, options)) {
            return 1;
        }

        TrEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return TrApp::run(argc, argv);
}
