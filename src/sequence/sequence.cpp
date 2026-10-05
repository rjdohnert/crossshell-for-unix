/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, cmd-extended contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
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
 * SINGLE FILE INDEX: sequence.cpp
 * ============================================================================
 * Sequence - Object-Oriented Sort & Stream Ordering Engine
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [KEY FLAGS & SPECIFICATION] .......... KeyFlags and KeyDefinition structures
 * 2. [OPTIONS PARSER & HELP ENGINE] ........ SequenceOptions class (CLI parsing & flags)
 * 3. [FIELD EXTRACTOR ENGINE] .............. FieldExtractor class (column/character slicing)
 * 4. [VALUE COMPARATORS & TRANSFORMERS] .... ValueTransformer and KeyComparator classes
 * 5. [STREAM PIPELINE & SORT ENGINE] ....... SequenceEngine class (I/O, sort, unique, check)
 * 6. [APPLICATION CONTROLLER] .............. SequenceApp class and main entry point
 * ============================================================================
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <random>
#include <chrono>
#include <unordered_map>
#include <optional>
#include <memory>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

// ============================================================================
// 1. KEY FLAGS & SPECIFICATION
// ============================================================================

struct KeyFlags {
    bool numeric{false};             // -n
    bool humanNumeric{false};        // -h
    bool generalNumeric{false};      // -g
    bool month{false};               // -M
    bool version{false};             // -V
    bool randomSort{false};          // -R
    bool reverse{false};             // -r
    bool ignoreCase{false};          // -f
    bool dictOrder{false};           // -d
    bool ignoreNonPrint{false};      // -i
    bool ignoreLeadingBlanks{false}; // -b
};

class KeyDefinition {
public:
    int startField{1};
    int startChar{1};
    int endField{0}; // 0 = end of line
    int endChar{0};  // 0 = end of field
    KeyFlags flags;
    bool hasCustomFlags{false};

    static bool parse(const std::string& def, KeyDefinition& outKey, const KeyFlags& globalFlags) {
        outKey.flags = globalFlags;
        size_t comma = def.find(',');
        std::string startPart = (comma == std::string::npos) ? def : def.substr(0, comma);
        std::string endPart = (comma == std::string::npos) ? "" : def.substr(comma + 1);

        auto parsePart = [](const std::string& p, int& f, int& c, KeyFlags& kflags, bool& custom) {
            size_t idx = 0;
            f = 0; c = 0;
            while (idx < p.size() && std::isdigit(static_cast<unsigned char>(p[idx]))) {
                f = f * 10 + (p[idx] - '0');
                ++idx;
            }
            if (f == 0) f = 1;
            if (idx < p.size() && p[idx] == '.') {
                ++idx;
                while (idx < p.size() && std::isdigit(static_cast<unsigned char>(p[idx]))) {
                    c = c * 10 + (p[idx] - '0');
                    ++idx;
                }
            }
            while (idx < p.size()) {
                custom = true;
                char opt = p[idx++];
                switch (opt) {
                    case 'n': kflags.numeric = true; break;
                    case 'h': kflags.humanNumeric = true; break;
                    case 'g': kflags.generalNumeric = true; break;
                    case 'M': kflags.month = true; break;
                    case 'V': kflags.version = true; break;
                    case 'r': kflags.reverse = true; break;
                    case 'f': kflags.ignoreCase = true; break;
                    case 'd': kflags.dictOrder = true; break;
                    case 'i': kflags.ignoreNonPrint = true; break;
                    case 'b': kflags.ignoreLeadingBlanks = true; break;
                    default: break;
                }
            }
        };

        parsePart(startPart, outKey.startField, outKey.startChar, outKey.flags, outKey.hasCustomFlags);
        if (!endPart.empty()) {
            parsePart(endPart, outKey.endField, outKey.endChar, outKey.flags, outKey.hasCustomFlags);
        }
        return true;
    }
};

// ============================================================================
// 2. OPTIONS PARSER & HELP ENGINE
// ============================================================================

class SequenceOptions {
public:
    KeyFlags globalFlags;
    std::vector<KeyDefinition> keys;
    char delimiter{'\0'}; // '\0' = whitespace transition
    char lineTerminator{'\n'};
    bool unique{false};
    bool stable{true};
    bool checkOnly{false};
    bool checkSilent{false};
    std::string outputFile{""};
    std::vector<std::string> inputFiles;

    static void showHelp() {
        std::cout << R"(sequence(1)             CrossShell for UNIX Reference Manual                sequence(1)

    NAME
        sequence - sort concatenated input records

    SYNOPSIS
        sequence [OPTIONS] [FILE]...

    DESCRIPTION
        Reads files or standard input, concatenates the records, and writes sorted
        output. Windows streams are binary-configured; '-' denotes standard input.

    OPTIONS
        -b, --ignore-leading-blanks
            Ignore leading blanks.

        -d, --dictionary-order
            Use dictionary order.

        -f, --ignore-case
            Compare without case distinctions.

        -g, --general-numeric-sort
            Sort general numeric values.

        -h, --human-numeric-sort
            Sort human-readable numeric values.

        -i, --ignore-nonprinting
            Ignore nonprinting characters.

        -M, --month-sort
            Sort by month names.

        -n, --numeric-sort
            Sort numerically.

        -R, --random-sort
            Deterministically shuffle records.

        -r, --reverse
            Reverse the sort order.

        -V, --version-sort
            Sort version strings.

        -c, --check
            Check whether input is sorted.

        -C, --check=quiet
            Check silently.

        -k, --key KEYDEF
            Sort by a field/character key.

        -o, --output FILE
            Write sorted output to FILE.

        -s, --stable
            Enable stable sorting.

        -t, --field-separator SEP
            Set the field separator.

        -u, --unique
            Remove equal adjacent output keys.

        -z, --zero-terminated
            Use NUL instead of newline records.

        --help
            Display this reference manual.

        --version
            Display version information and exit.

    EXAMPLES
        sequence -u names.txt
            Sort names.txt removing duplicate lines.

        sequence -t: -k3,3n /etc/passwd
            Sort passwd entries numerically by third field.

        sequence -h -r -k2 disk_usage.txt -o sorted_usage.txt
            Sort disk usage by second field in human-numeric reverse order.

    EXIT STATUS
        0          Help, version, successful sorting, or ordered input.
        1          Check mode detected disorder.
        2          Invalid parsing or input/output file failure.

    CrossShell for UNIX                                                    sequence(1)
    )";
    }

    static void showVersion() {
        std::cout << "sequence 3.0.7\n";
        std::cout << "Licensed under the BSD-3-Clause License\n";
    }

    static bool parse(int argc, char* argv[], SequenceOptions& opt) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-?" || arg == "/?") { showHelp(); std::exit(0); }
            if (arg == "--version") { showVersion(); std::exit(0); }

            if (arg.rfind("--", 0) == 0) {
                if (arg == "--ignore-leading-blanks") opt.globalFlags.ignoreLeadingBlanks = true;
                else if (arg == "--dictionary-order") opt.globalFlags.dictOrder = true;
                else if (arg == "--ignore-case") opt.globalFlags.ignoreCase = true;
                else if (arg == "--general-numeric-sort") opt.globalFlags.generalNumeric = true;
                else if (arg == "--human-numeric-sort") opt.globalFlags.humanNumeric = true;
                else if (arg == "--ignore-nonprinting") opt.globalFlags.ignoreNonPrint = true;
                else if (arg == "--month-sort") opt.globalFlags.month = true;
                else if (arg == "--numeric-sort") opt.globalFlags.numeric = true;
                else if (arg == "--random-sort") opt.globalFlags.randomSort = true;
                else if (arg == "--reverse") opt.globalFlags.reverse = true;
                else if (arg == "--version-sort") opt.globalFlags.version = true;
                else if (arg == "--unique") opt.unique = true;
                else if (arg == "--stable") opt.stable = true;
                else if (arg == "--zero-terminated") opt.lineTerminator = '\0';
                else if (arg == "--check" || arg == "--check=diagnose-first") opt.checkOnly = true;
                else if (arg == "--check=quiet" || arg == "--check=silent") { opt.checkOnly = true; opt.checkSilent = true; }
                else if (arg.rfind("--output=", 0) == 0) opt.outputFile = arg.substr(9);
                else if (arg.rfind("--field-separator=", 0) == 0) opt.delimiter = arg.substr(18)[0];
                else if (arg.rfind("--key=", 0) == 0) {
                    KeyDefinition ks;
                    KeyDefinition::parse(arg.substr(6), ks, opt.globalFlags);
                    opt.keys.push_back(ks);
                }
                continue;
            }

            if (arg.size() > 1 && arg[0] == '-') {
                for (size_t j = 1; j < arg.size(); ++j) {
                    char c = arg[j];
                    switch (c) {
                        case 'b': opt.globalFlags.ignoreLeadingBlanks = true; break;
                        case 'd': opt.globalFlags.dictOrder = true; break;
                        case 'f': opt.globalFlags.ignoreCase = true; break;
                        case 'g': opt.globalFlags.generalNumeric = true; break;
                        case 'h': opt.globalFlags.humanNumeric = true; break;
                        case 'i': opt.globalFlags.ignoreNonPrint = true; break;
                        case 'M': opt.globalFlags.month = true; break;
                        case 'n': opt.globalFlags.numeric = true; break;
                        case 'R': opt.globalFlags.randomSort = true; break;
                        case 'r': opt.globalFlags.reverse = true; break;
                        case 'V': opt.globalFlags.version = true; break;
                        case 'u': opt.unique = true; break;
                        case 's': opt.stable = true; break;
                        case 'z': opt.lineTerminator = '\0'; break;
                        case 'c': opt.checkOnly = true; break;
                        case 'C': opt.checkOnly = true; opt.checkSilent = true; break;
                        case 'o':
                            if (j + 1 < arg.size()) opt.outputFile = arg.substr(j + 1);
                            else if (++i < argc) opt.outputFile = argv[i];
                            j = arg.size();
                            break;
                        case 't':
                            if (j + 1 < arg.size()) opt.delimiter = arg[j + 1];
                            else if (++i < argc) opt.delimiter = argv[i][0];
                            j = arg.size();
                            break;
                        case 'k': {
                            std::string karg;
                            if (j + 1 < arg.size()) karg = arg.substr(j + 1);
                            else if (++i < argc) karg = argv[i];
                            KeyDefinition ks;
                            KeyDefinition::parse(karg, ks, opt.globalFlags);
                            opt.keys.push_back(ks);
                            j = arg.size();
                            break;
                        }
                        default:
                            std::cerr << "sequence: invalid option -- '" << c << "'\n";
                            std::cerr << "Try 'sequence --help' for more information.\n";
                            return false;
                    }
                }
                continue;
            }

            opt.inputFiles.push_back(arg);
        }

        if (opt.inputFiles.empty()) {
            opt.inputFiles.push_back("-");
        }

        return true;
    }
};

// ============================================================================
// 3. FIELD EXTRACTOR ENGINE
// ============================================================================

class FieldExtractor {
public:
    static std::string_view extractKey(const std::string& line, const KeyDefinition& key, char delimiter) {
        std::vector<std::pair<size_t, size_t>> fields;
        size_t n = line.size();

        if (delimiter == '\0') {
            size_t i = 0;
            while (i < n) {
                while (i < n && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
                if (i >= n) break;
                size_t start = i;
                while (i < n && !std::isspace(static_cast<unsigned char>(line[i]))) ++i;
                fields.emplace_back(start, i - start);
            }
        } else {
            size_t start = 0;
            for (size_t i = 0; i <= n; ++i) {
                if (i == n || line[i] == delimiter) {
                    fields.emplace_back(start, i - start);
                    start = i + 1;
                }
            }
        }

        if (fields.empty()) return std::string_view(line.data(), 0);

        int sfIdx = key.startField - 1;
        if (sfIdx >= static_cast<int>(fields.size())) return std::string_view(line.data(), 0);
        size_t charStart = fields[sfIdx].first + std::max(0, key.startChar - 1);
        if (charStart >= line.size()) return std::string_view(line.data(), 0);

        size_t charEnd = line.size();
        if (key.endField > 0) {
            int efIdx = key.endField - 1;
            if (efIdx < static_cast<int>(fields.size())) {
                if (key.endChar > 0) {
                    charEnd = fields[efIdx].first + key.endChar;
                } else {
                    charEnd = fields[efIdx].first + fields[efIdx].second;
                }
            }
        }

        if (charStart >= charEnd) return std::string_view(line.data(), 0);
        return std::string_view(line.data() + charStart, std::min(charEnd, line.size()) - charStart);
    }
};

// ============================================================================
// 4. VALUE COMPARATORS & TRANSFORMERS
// ============================================================================

class ValueTransformer {
public:
    static std::string transform(std::string_view sv, const KeyFlags& flags) {
        std::string res;
        res.reserve(sv.size());
        size_t start = 0;
        if (flags.ignoreLeadingBlanks) {
            while (start < sv.size() && std::isspace(static_cast<unsigned char>(sv[start]))) ++start;
        }
        for (size_t i = start; i < sv.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(sv[i]);
            if (flags.dictOrder && !std::isalnum(c) && !std::isspace(c)) continue;
            if (flags.ignoreNonPrint && !std::isprint(c)) continue;
            if (flags.ignoreCase) c = std::tolower(c);
            res.push_back(static_cast<char>(c));
        }
        return res;
    }
};

class KeyComparator {
private:
    static inline const std::unordered_map<std::string, int> MONTH_MAP = {
        {"JAN", 1}, {"FEB", 2}, {"MAR", 3}, {"APR", 4}, {"MAY", 5}, {"JUN", 6},
        {"JUL", 7}, {"AUG", 8}, {"SEP", 9}, {"OCT", 10}, {"NOV", 11}, {"DEC", 12}
    };

public:
    static int parseMonth(std::string_view s) {
        size_t i = 0;
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        if (i + 3 <= s.size()) {
            std::string m = "";
            for (int k = 0; k < 3; ++k) m += std::toupper(static_cast<unsigned char>(s[i + k]));
            auto it = MONTH_MAP.find(m);
            if (it != MONTH_MAP.end()) return it->second;
        }
        return 0;
    }

    static double parseHumanNumber(std::string_view s) {
        size_t i = 0;
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        char* endptr = nullptr;
        std::string str(s.substr(i));
        double val = std::strtod(str.c_str(), &endptr);
        if (endptr && *endptr) {
            char unit = std::toupper(static_cast<unsigned char>(*endptr));
            switch (unit) {
                case 'K': val *= 1024.0; break;
                case 'M': val *= 1024.0 * 1024.0; break;
                case 'G': val *= 1024.0 * 1024.0 * 1024.0; break;
                case 'T': val *= 1024.0 * 1024.0 * 1024.0 * 1024.0; break;
                case 'P': val *= 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0; break;
                case 'E': val *= 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0; break;
                default: break;
            }
        }
        return val;
    }

    static int compareVersion(std::string_view a, std::string_view b) {
        size_t i = 0, j = 0;
        while (i < a.size() || j < b.size()) {
            if (i < a.size() && j < b.size() && std::isdigit(static_cast<unsigned char>(a[i])) && std::isdigit(static_cast<unsigned char>(b[j]))) {
                while (i < a.size() && a[i] == '0') ++i;
                while (j < b.size() && b[j] == '0') ++j;
                size_t startI = i, startJ = j;
                while (i < a.size() && std::isdigit(static_cast<unsigned char>(a[i]))) ++i;
                while (j < b.size() && std::isdigit(static_cast<unsigned char>(b[j]))) ++j;
                size_t lenA = i - startI, lenB = j - startJ;
                if (lenA != lenB) return (lenA < lenB) ? -1 : 1;
                int cmp = a.substr(startI, lenA).compare(b.substr(startJ, lenB));
                if (cmp != 0) return cmp;
            } else {
                char ca = (i < a.size()) ? a[i] : 0;
                char cb = (j < b.size()) ? b[j] : 0;
                if (ca != cb) return (ca < cb) ? -1 : 1;
                if (i < a.size()) ++i;
                if (j < b.size()) ++j;
            }
        }
        return 0;
    }

    static int compare(std::string_view a, std::string_view b, const KeyFlags& flags) {
        int cmp = 0;
        if (flags.numeric) {
            char* ea = nullptr; char* eb = nullptr;
            std::string sa(a), sb(b);
            long double va = std::strtold(sa.c_str(), &ea);
            long double vb = std::strtold(sb.c_str(), &eb);
            if (va < vb) cmp = -1;
            else if (va > vb) cmp = 1;
            else cmp = 0;
        } else if (flags.generalNumeric) {
            char* ea = nullptr; char* eb = nullptr;
            std::string sa(a), sb(b);
            double va = std::strtod(sa.c_str(), &ea);
            double vb = std::strtod(sb.c_str(), &eb);
            if (va < vb) cmp = -1;
            else if (va > vb) cmp = 1;
            else cmp = 0;
        } else if (flags.humanNumeric) {
            double va = parseHumanNumber(a);
            double vb = parseHumanNumber(b);
            if (va < vb) cmp = -1;
            else if (va > vb) cmp = 1;
            else cmp = 0;
        } else if (flags.month) {
            int ma = parseMonth(a);
            int mb = parseMonth(b);
            cmp = (ma < mb) ? -1 : (ma > mb ? 1 : 0);
        } else if (flags.version) {
            cmp = compareVersion(a, b);
        } else {
            std::string ta = ValueTransformer::transform(a, flags);
            std::string tb = ValueTransformer::transform(b, flags);
            cmp = ta.compare(tb);
        }
        return flags.reverse ? -cmp : cmp;
    }

    static bool lineLess(const std::string& a, const std::string& b, const SequenceOptions& opt) {
        if (!opt.keys.empty()) {
            for (const auto& k : opt.keys) {
                std::string_view ka = FieldExtractor::extractKey(a, k, opt.delimiter);
                std::string_view kb = FieldExtractor::extractKey(b, k, opt.delimiter);
                const KeyFlags& f = k.hasCustomFlags ? k.flags : opt.globalFlags;
                int c = compare(ka, kb, f);
                if (c != 0) return c < 0;
            }
        }
        int fallback = compare(a, b, opt.globalFlags);
        return fallback < 0;
    }

    static bool lineEqual(const std::string& a, const std::string& b, const SequenceOptions& opt) {
        return !lineLess(a, b, opt) && !lineLess(b, a, opt);
    }
};

// ============================================================================
// 5. STREAM PIPELINE & SORT ENGINE
// ============================================================================

class SequenceEngine {
private:
    SequenceOptions options;

public:
    explicit SequenceEngine(SequenceOptions opts) : options(std::move(opts)) {}

    int execute() {
        #ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
        #endif

        std::vector<std::string> lines;
        for (const auto& file : options.inputFiles) {
            std::istream* stream = &std::cin;
            std::ifstream fileStream;
            if (file != "-") {
                fileStream.open(file, std::ios::binary);
                if (!fileStream.is_open()) {
                    std::cerr << "sequence: open failed: " << file << ": No such file or directory\n";
                    return 2;
                }
                stream = &fileStream;
            }

            std::string line;
            while (std::getline(*stream, line, options.lineTerminator)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                lines.push_back(std::move(line));
            }
        }

        // Check-only mode (-c / -C)
        if (options.checkOnly) {
            for (size_t i = 1; i < lines.size(); ++i) {
                bool disorder = options.unique ? !KeyComparator::lineLess(lines[i - 1], lines[i], options)
                                               : KeyComparator::lineLess(lines[i], lines[i - 1], options);
                if (disorder) {
                    if (!options.checkSilent) {
                        std::cerr << "sequence: disorder on line " << (i + 1) << ": " << lines[i] << "\n";
                    }
                    return 1;
                }
            }
            return 0;
        }

        // Random sort or stable sort
        if (options.globalFlags.randomSort) {
            std::mt19937 g(1337);
            std::shuffle(lines.begin(), lines.end(), g);
        } else {
            std::stable_sort(lines.begin(), lines.end(), [&](const std::string& a, const std::string& b) {
                return KeyComparator::lineLess(a, b, options);
            });
        }

        // Output stream
        std::ostream* out = &std::cout;
        std::ofstream outFileStream;
        if (!options.outputFile.empty()) {
            outFileStream.open(options.outputFile, std::ios::binary);
            if (!outFileStream.is_open()) {
                std::cerr << "sequence: open failed: " << options.outputFile << "\n";
                return 2;
            }
            out = &outFileStream;
        }

        // Write output (with optional -u unique filter)
        for (size_t i = 0; i < lines.size(); ++i) {
            if (options.unique && i > 0 && KeyComparator::lineEqual(lines[i - 1], lines[i], options)) {
                continue;
            }
            *out << lines[i] << options.lineTerminator;
        }

        return 0;
    }
};

// ============================================================================
// 6. APPLICATION CONTROLLER
// ============================================================================

class SequenceApp {
public:
    static int run(int argc, char* argv[]) {
        SequenceOptions options;
        if (!SequenceOptions::parse(argc, argv, options)) {
            return 2;
        }

        SequenceEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return SequenceApp::run(argc, argv);
}