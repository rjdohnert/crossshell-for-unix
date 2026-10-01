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
 * SINGLE FILE INDEX: nl.cpp
 * ============================================================================
 * WinNl - Object-Oriented Line Numbering Filter for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & STYLE STRUCTURES] .......... NlStyle and NlOptions classes (CLI parsing)
 * 2. [LINE FORMATTER ENGINE] ............... LineNumberFormatter class (LN, RN, RZ numbering)
 * 3. [CORE NUMBERING ENGINE] ............... NlEngine class (section delimiters & stream output)
 * 4. [APPLICATION CONTROLLER] .............. NlApp class and main entry point
 * ============================================================================
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <sstream>
#include <iomanip>
#include <memory>
#include <cctype>

// ============================================================================
// 1. OPTIONS & STYLE STRUCTURES
// ============================================================================

enum class StyleMode { ALL, NON_EMPTY, NONE, REGEX };
enum class NumberFormat { LN, RN, RZ }; // Left, Right, Right Zero-Padded
enum class SectionType { HEADER, BODY, FOOTER };

struct NlStyle {
    StyleMode mode{StyleMode::NON_EMPTY};
    std::regex pattern;
    std::string rawPattern;

    static NlStyle parse(const std::string& arg) {
        NlStyle s;
        if (arg.empty()) return s;

        char c = arg[0];
        if (c == 'a') {
            s.mode = StyleMode::ALL;
        } else if (c == 't') {
            s.mode = StyleMode::NON_EMPTY;
        } else if (c == 'n') {
            s.mode = StyleMode::NONE;
        } else if (c == 'p') {
            s.mode = StyleMode::REGEX;
            s.rawPattern = arg.substr(1);
            try {
                s.pattern = std::regex(s.rawPattern);
            } catch (const std::regex_error& e) {
                std::cerr << "nl: invalid regular expression '" << s.rawPattern << "': " << e.what() << "\n";
                std::exit(1);
            }
        } else {
            std::cerr << "nl: unknown style type: " << arg << "\n";
            std::exit(1);
        }
        return s;
    }
};

class NlOptions {
public:
    NlStyle bodyStyle{StyleMode::NON_EMPTY, {}, ""};
    NlStyle headerStyle{StyleMode::NONE, {}, ""};
    NlStyle footerStyle{StyleMode::NONE, {}, ""};

    std::string delim{"\\:"};
    long long startNum{1};
    long long increment{1};
    bool renumberPerPage{true};
    int blankLinesLimit{1};
    std::string separator{"\t"};
    int width{6};
    NumberFormat format{NumberFormat::RN};
    std::vector<std::string> files;

    static void printHelp() {
        std::cout << "Usage: nl [OPTION]... [FILE]...\n"
                  << "Write each FILE to standard output, with line numbers added.\n"
                  << "With no FILE, or when FILE is -, read standard input.\n\n"
                  << "Options:\n"
                  << "  -b, --body-numbering=STYLE   use STYLE for numbering body lines\n"
                  << "  -d, --section-delimiter=CC   use CC for logical page delimiters\n"
                  << "  -f, --footer-numbering=STYLE use STYLE for numbering footer lines\n"
                  << "  -h, --header-numbering=STYLE use STYLE for numbering header lines\n"
                  << "  -i, --line-increment=NUMBER  line number increment at each line\n"
                  << "  -l, --join-blank-lines=NUMBER group of NUMBER empty lines counted as one\n"
                  << "  -n, --number-format=FORMAT   insert line numbers according to FORMAT\n"
                  << "  -p, --no-renumber            do not reset line numbers for each section\n"
                  << "  -s, --number-separator=STRING add STRING after line number\n"
                  << "  -v, --starting-line-number=NUMBER first line number for each section\n"
                  << "  -w, --number-width=NUMBER    use NUMBER columns for line numbers\n"
                  << "      --help                   display this help and exit\n"
                  << "      --version                output version information and exit\n\n"
                  << "FORMAT is one of: ln (left justified), rn (right justified), rz (right justified with leading zeroes).\n"
                  << "STYLE is one of: a (all lines), t (non-empty lines only), n (no lines), pBRE (only lines matching BRE).\n";
    }

    static void printVersion() {
        std::cout << "nl 1.0\n";
    }

    static bool parse(int argc, char* argv[], NlOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help") {
                printHelp();
                std::exit(0);
            } else if (arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-p" || arg == "--no-renumber") {
                opts.renumberPerPage = false;
            } else if (arg.rfind("-b", 0) == 0) {
                opts.bodyStyle = NlStyle::parse(arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : ""));
            } else if (arg.rfind("-h", 0) == 0 && arg != "-h") {
                opts.headerStyle = NlStyle::parse(arg.substr(2));
            } else if (arg == "-h") {
                if (i + 1 < argc && argv[i + 1][0] != '-') opts.headerStyle = NlStyle::parse(argv[++i]);
                else { printHelp(); std::exit(0); }
            } else if (arg.rfind("-f", 0) == 0) {
                opts.footerStyle = NlStyle::parse(arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : ""));
            } else if (arg.rfind("-d", 0) == 0) {
                std::string d = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "");
                opts.delim = (d.size() == 1) ? d + ":" : d;
            } else if (arg.rfind("-v", 0) == 0) {
                opts.startNum = std::stoll(arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "1"));
            } else if (arg.rfind("-i", 0) == 0) {
                opts.increment = std::stoll(arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "1"));
            } else if (arg.rfind("-l", 0) == 0) {
                opts.blankLinesLimit = std::stoi(arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "1"));
            } else if (arg.rfind("-s", 0) == 0) {
                opts.separator = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "\t");
            } else if (arg.rfind("-w", 0) == 0) {
                opts.width = std::stoi(arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "6"));
            } else if (arg.rfind("-n", 0) == 0) {
                std::string fmt = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "rn");
                if (fmt == "ln") opts.format = NumberFormat::LN;
                else if (fmt == "rn") opts.format = NumberFormat::RN;
                else if (fmt == "rz") opts.format = NumberFormat::RZ;
            } else if (arg[0] == '-' && arg.size() > 1) {
                std::cerr << "nl: unrecognized option '" << arg << "'\n";
                return false;
            } else {
                opts.files.push_back(arg);
            }
        }
        if (opts.files.empty()) opts.files.push_back("-");
        return true;
    }
};

// ============================================================================
// 2. LINE FORMATTER ENGINE
// ============================================================================

class LineNumberFormatter {
public:
    static std::string format(long long num, int width, NumberFormat fmt) {
        std::ostringstream ss;
        if (fmt == NumberFormat::LN) {
            ss << std::left << std::setw(width) << num;
        } else if (fmt == NumberFormat::RN) {
            ss << std::right << std::setw(width) << num;
        } else if (fmt == NumberFormat::RZ) {
            ss << std::right << std::setw(width) << std::setfill('0') << num;
        }
        return ss.str();
    }
};

// ============================================================================
// 3. CORE NUMBERING ENGINE
// ============================================================================

class NlEngine {
private:
    NlOptions options;

    static bool shouldNumber(const std::string& line, const NlStyle& style, int& blankCount, int blankLimit) {
        if (style.mode == StyleMode::NONE) return false;
        if (style.mode == StyleMode::ALL) {
            if (line.empty()) {
                blankCount++;
                if (blankCount >= blankLimit) {
                    blankCount = 0;
                    return true;
                }
                return false;
            }
            blankCount = 0;
            return true;
        }
        if (style.mode == StyleMode::NON_EMPTY) {
            return !line.empty();
        }
        if (style.mode == StyleMode::REGEX) {
            return std::regex_search(line, style.pattern);
        }
        return false;
    }

    void processStream(std::istream& in) const {
        std::string line;
        long long currentNum = options.startNum;
        SectionType currentSec = SectionType::BODY;
        int blankCount = 0;

        std::string hDelim = options.delim + options.delim + options.delim;
        std::string bDelim = options.delim + options.delim;
        std::string fDelim = options.delim;

        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();

            if (line == hDelim) {
                currentSec = SectionType::HEADER;
                if (options.renumberPerPage) currentNum = options.startNum;
                std::cout << "\n";
                continue;
            } else if (line == bDelim) {
                currentSec = SectionType::BODY;
                if (options.renumberPerPage) currentNum = options.startNum;
                std::cout << "\n";
                continue;
            } else if (line == fDelim) {
                currentSec = SectionType::FOOTER;
                if (options.renumberPerPage) currentNum = options.startNum;
                std::cout << "\n";
                continue;
            }

            const NlStyle* activeStyle = &options.bodyStyle;
            if (currentSec == SectionType::HEADER) activeStyle = &options.headerStyle;
            else if (currentSec == SectionType::FOOTER) activeStyle = &options.footerStyle;

            if (shouldNumber(line, *activeStyle, blankCount, options.blankLinesLimit)) {
                std::cout << LineNumberFormatter::format(currentNum, options.width, options.format)
                          << options.separator << line << "\n";
                currentNum += options.increment;
            } else {
                std::cout << std::string(options.width, ' ') << options.separator << line << "\n";
            }
        }
    }

public:
    explicit NlEngine(NlOptions opts) : options(std::move(opts)) {}

    int execute() {
        for (const auto& file : options.files) {
            if (file == "-") {
                processStream(std::cin);
            } else {
                std::ifstream infile(file);
                if (!infile.is_open()) {
                    std::cerr << "nl: " << file << ": No such file or directory\n";
                    return 1;
                }
                processStream(infile);
            }
        }
        return 0;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class NlApp {
public:
    static int run(int argc, char* argv[]) {
        NlOptions options;
        if (!NlOptions::parse(argc, argv, options)) {
            return 1;
        }
        NlEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return NlApp::run(argc, argv);
}
