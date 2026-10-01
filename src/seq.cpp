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
 * SINGLE FILE INDEX: seq.cpp
 * ============================================================================
 * WinSeq - Object-Oriented Sequence Number Generator for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [DATA STRUCTURES & NUMBER PARSER] ..... NumberInfo struct and precision analysis
 * 2. [OPTIONS PARSER & HELP ENGINE] ........ SeqOptions class, CLI argument parsing
 * 3. [NUMBER FORMATTING ENGINE] ............ NumberFormatter class (printf/equal-width)
 * 4. [CORE SEQUENCE ENGINE] ................ SeqEngine class, iteration and step generator
 * 5. [APPLICATION CONTROLLER] .............. SeqApp class and main entry point
 * ============================================================================
 */

#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <algorithm>
#include <cctype>
#include <memory>
#include <stdexcept>

// ============================================================================
// 1. DATA STRUCTURES & NUMBER PARSER
// ============================================================================

struct NumberInfo {
    std::string raw;
    double value{0.0};
    int intWidth{0};
    int fracWidth{0};
};

class NumberParser {
public:
    static std::string unescape(const std::string& input) {
        std::string res;
        for (size_t i = 0; i < input.length(); ++i) {
            if (input[i] == '\\' && i + 1 < input.length()) {
                switch (input[i + 1]) {
                    case 'n': res += '\n'; ++i; break;
                    case 't': res += '\t'; ++i; break;
                    case 'r': res += '\r'; ++i; break;
                    case '\\': res += '\\'; ++i; break;
                    default: res += input[i]; break;
                }
            } else {
                res += input[i];
            }
        }
        return res;
    }

    static NumberInfo parse(const std::string& str) {
        NumberInfo info;
        info.raw = str;
        info.value = std::stod(str);

        size_t start = (str[0] == '-' || str[0] == '+') ? 1 : 0;
        size_t dotPos = str.find('.', start);

        if (dotPos == std::string::npos) {
            info.intWidth = static_cast<int>(str.length() - start);
            info.fracWidth = 0;
        } else {
            info.intWidth = static_cast<int>(dotPos - start);
            info.fracWidth = static_cast<int>(str.length() - dotPos - 1);
        }
        return info;
    }
};

// ============================================================================
// 2. OPTIONS PARSER & HELP ENGINE
// ============================================================================

class SeqOptions {
public:
    std::string separator{"\n"};
    std::string customFormat{""};
    bool equalWidth{false};
    std::string startStr{"1"};
    std::string incrementStr{"1"};
    std::string lastStr{""};

    static void printUsage(const char* progName) {
        std::cout << R"(seq(1)                  CrossShell for UNIX Reference Manual                       seq(1)

    NAME
        seq - print a numeric sequence

    SYNOPSIS
        seq [OPTIONS] LAST
        seq [OPTIONS] FIRST LAST
        seq [OPTIONS] FIRST INCREMENT LAST

    DESCRIPTION
        Prints numbers from FIRST through LAST using INCREMENT. Negative numeric
        values are accepted as positional values, and a direction mismatch produces
        no sequence with a successful exit status.

    OPTIONS
        -f, --format FORMAT
            Use a printf-style floating-point format.

        -s, --separator STRING
            Set the separator; default is newline.

        -w, --equal-width
            Pad values with leading zeroes.

        -h, --help, /?
            Display this reference manual.

        --version
            Display version information and exit.

        --
            End options before numeric operands.

    EXAMPLES
        seq 5
            Print sequence from 1 to 5.

        seq 2 5
            Print sequence from 2 to 5.

        seq 1 0.5 3
            Print sequence with 0.5 increment.

        seq -s, 1 3
            Print comma-separated sequence.

        seq -w 8 10
            Print zero-padded sequence.

    EXIT STATUS
        0          Help, version, direction mismatch, or successful generation.
        1          Invalid option/number, missing argument, or zero increment.

    CrossShell for UNIX                                                          seq(1)
    )";
    }

    static void printVersion() {
        std::cout << "seq 1.0\n";
    }

    static bool parse(int argc, char* argv[], SeqOptions& outOpts) {
        std::vector<std::string> positional;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--") {
                for (++i; i < argc; ++i) {
                    positional.push_back(argv[i]);
                }
                break;
            } else if (arg == "-h" || arg == "--help" || arg == "/?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-s" || arg == "--separator") {
                if (i + 1 < argc) outOpts.separator = NumberParser::unescape(argv[++i]);
                else { std::cerr << "seq: option requires an argument -- 's'\n"; return false; }
            } else if (arg.rfind("--separator=", 0) == 0) {
                outOpts.separator = NumberParser::unescape(arg.substr(12));
            } else if (arg.rfind("-s", 0) == 0 && arg.size() > 2) {
                outOpts.separator = NumberParser::unescape(arg.substr(2));
            } else if (arg == "-f" || arg == "--format") {
                if (i + 1 < argc) outOpts.customFormat = argv[++i];
                else { std::cerr << "seq: option requires an argument -- 'f'\n"; return false; }
            } else if (arg.rfind("--format=", 0) == 0) {
                outOpts.customFormat = arg.substr(9);
            } else if (arg.rfind("-f", 0) == 0 && arg.size() > 2) {
                outOpts.customFormat = arg.substr(2);
            } else if (arg == "-w" || arg == "--equal-width") {
                outOpts.equalWidth = true;
            } else if (!arg.empty() && arg[0] == '-' && arg.length() > 1 && (std::isdigit(static_cast<unsigned char>(arg[1])) || arg[1] == '.')) {
                positional.push_back(arg);
            } else if (!arg.empty() && arg[0] == '-') {
                std::cerr << "seq: invalid option '" << arg << "'\n";
                return false;
            } else {
                positional.push_back(arg);
            }
        }

        if (positional.empty() || positional.size() > 3) {
            std::cerr << "seq: invalid number of arguments\n";
            std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
            return false;
        }

        if (positional.size() == 1) {
            outOpts.startStr = "1";
            outOpts.incrementStr = "1";
            outOpts.lastStr = positional[0];
        } else if (positional.size() == 2) {
            outOpts.startStr = positional[0];
            outOpts.incrementStr = "1";
            outOpts.lastStr = positional[1];
        } else {
            outOpts.startStr = positional[0];
            outOpts.incrementStr = positional[1];
            outOpts.lastStr = positional[2];
        }

        return true;
    }
};

// ============================================================================
// 3. NUMBER FORMATTING ENGINE
// ============================================================================

class NumberFormatter {
private:
    std::string customFormat;
    bool equalWidth;
    int maxIntWidth;
    int maxFracWidth;

public:
    NumberFormatter(std::string fmt, bool eqWidth, int maxInt, int maxFrac)
        : customFormat(std::move(fmt)), equalWidth(eqWidth), maxIntWidth(maxInt), maxFracWidth(maxFrac) {}

    void formatAndPrint(double val, std::ostream& out) const {
        if (!customFormat.empty()) {
            char buf[256];
            snprintf(buf, sizeof(buf), customFormat.c_str(), val);
            out << buf;
        } else if (equalWidth) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(maxFracWidth) << val;
            std::string strVal = ss.str();

            bool isNeg = (val < 0);
            std::string absStr = isNeg ? strVal.substr(1) : strVal;

            int totalWidth = maxIntWidth + (maxFracWidth > 0 ? maxFracWidth + 1 : 0);
            int padLen = totalWidth - static_cast<int>(absStr.length());

            if (isNeg) out << "-";
            if (padLen > 0) out << std::string(padLen, '0');
            out << absStr;
        } else {
            if (maxFracWidth == 0) {
                out << static_cast<long long>(std::round(val));
            } else {
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(maxFracWidth) << val;
                out << ss.str();
            }
        }
    }
};

// ============================================================================
// 4. CORE SEQUENCE ENGINE
// ============================================================================

class SeqEngine {
private:
    SeqOptions options;

public:
    explicit SeqEngine(SeqOptions opts) : options(std::move(opts)) {}

    int execute(std::ostream& out) {
        try {
            NumberInfo nFirst = NumberParser::parse(options.startStr);
            NumberInfo nIncr  = NumberParser::parse(options.incrementStr);
            NumberInfo nLast  = NumberParser::parse(options.lastStr);

            double first = nFirst.value;
            double incr  = nIncr.value;
            double last  = nLast.value;

            if (incr == 0.0) {
                std::cerr << "seq: zero increment step\n";
                return 1;
            }

            if ((incr > 0 && first > last) || (incr < 0 && first < last)) {
                return 0;
            }

            int maxFrac = std::max({nFirst.fracWidth, nIncr.fracWidth, nLast.fracWidth});
            int maxInt  = std::max({nFirst.intWidth, nIncr.intWidth, nLast.intWidth});
            const double epsilon = std::max(1e-12, std::fabs(incr) * 1e-12);

            NumberFormatter formatter(options.customFormat, options.equalWidth, maxInt, maxFrac);

            bool wroteAny = false;
            double current = first;
            while ((incr > 0 && current <= last + epsilon) || (incr < 0 && current >= last - epsilon)) {
                if (wroteAny) {
                    out << options.separator;
                }
                wroteAny = true;

                formatter.formatAndPrint(current, out);
                current += incr;
            }
            out << "\n";

        } catch (const std::exception&) {
            std::cerr << "seq: invalid floating point argument\n";
            return 1;
        }

        return 0;
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class SeqApp {
public:
    static int run(int argc, char* argv[]) {
        SeqOptions options;
        if (!SeqOptions::parse(argc, argv, options)) {
            return 1;
        }

        SeqEngine engine(std::move(options));
        return engine.execute(std::cout);
    }
};

int main(int argc, char* argv[]) {
    return SeqApp::run(argc, argv);
}
