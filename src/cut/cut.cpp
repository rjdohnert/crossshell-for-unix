/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
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

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. RANGE & SELECTION MODEL
// ============================================================================
struct Range {
    int start;
    int end; // -1 indicates open-ended (up to the end of the line)
};

class Selection {
public:
    std::vector<Range> ranges;

    void AddRange(int start, int end) {
        ranges.push_back({start, end});
    }

    bool IsSelected(int index) const {
        for (const auto& r : ranges) {
            if (r.end == -1) {
                if (index >= r.start) return true;
            } else {
                if (index >= r.start && index <= r.end) return true;
            }
        }
        return false;
    }
};

class RangeParser {
public:
    static bool ParseList(const std::string& list_str, Selection& sel) {
        std::stringstream ss(list_str);
        std::string item;
        while (std::getline(ss, item, ',')) {
            if (item.empty()) return false;

            if (std::count(item.begin(), item.end(), '-') > 1) {
                return false;
            }

            size_t dash = item.find('-');
            if (dash == std::string::npos) {
                try {
                    int val = std::stoi(item);
                    if (val <= 0) return false;
                    sel.AddRange(val, val);
                } catch (...) {
                    return false;
                }
            } else {
                std::string start_str = item.substr(0, dash);
                std::string end_str = item.substr(dash + 1);
                int start = 1;
                int end = -1;

                if (!start_str.empty()) {
                    try {
                        start = std::stoi(start_str);
                        if (start <= 0) return false;
                    } catch (...) {
                        return false;
                    }
                }
                if (!end_str.empty()) {
                    try {
                        end = std::stoi(end_str);
                        if (end <= 0) return false;
                    } catch (...) {
                        return false;
                    }
                }

                if (start != -1 && end != -1 && start > end) {
                    return false;
                }
                sel.AddRange(start, end);
            }
        }
        return !sel.ranges.empty();
    }
};

// ============================================================================
// 2. CONFIGURATION OPTIONS
// ============================================================================
enum class CutMode {
    NONE,
    BYTES,
    CHARACTERS,
    FIELDS
};

struct CutOptions {
    CutMode mode = CutMode::NONE;
    std::string list_str;
    Selection selection;
    std::string delimiter = "\t";
    std::string output_delimiter;
    bool only_delimited = false;
    bool complement = false;
    int output_format = 0; // 0=raw, 1=json, 2=csv, 3=table
    std::string pipe_command;
    std::vector<std::string> files;
};

// ============================================================================
// 3. STREAM PROCESSOR
// ============================================================================
class StreamProcessor {
public:
    static void ProcessLine(const std::string& line, const CutOptions& opts, std::ostream& out) {
        char delim_char = opts.delimiter.empty() ? '\t' : opts.delimiter[0];
        std::string effective_out_delim = opts.output_delimiter.empty() ? std::string(1, delim_char) : opts.output_delimiter;

        if (opts.mode == CutMode::FIELDS) {
            size_t pos = line.find(delim_char);
            if (pos == std::string::npos) {
                if (!opts.only_delimited) {
                    out << line << "\n";
                }
                return;
            }

            std::vector<std::string> fields;
            size_t start = 0;
            size_t end = line.find(delim_char);
            while (end != std::string::npos) {
                fields.push_back(line.substr(start, end - start));
                start = end + 1;
                end = line.find(delim_char, start);
            }
            fields.push_back(line.substr(start));

            bool first = true;
            for (size_t idx = 0; idx < fields.size(); ++idx) {
                int field_idx = static_cast<int>(idx + 1);
                bool selected = opts.selection.IsSelected(field_idx);
                if (opts.complement) selected = !selected;

                if (selected) {
                    if (!first) out << effective_out_delim;
                    out << fields[idx];
                    first = false;
                }
            }
            out << "\n";
        } else {
            // BYTES or CHARACTERS mode
            std::string result;
            int total_items = static_cast<int>(line.size());
            for (int i = 1; i <= total_items; ++i) {
                bool selected = opts.selection.IsSelected(i);
                if (opts.complement) selected = !selected;
                if (selected) {
                    result += line[i - 1];
                }
            }
            out << result << "\n";
        }
    }

    static void ProcessStream(std::istream& in, const CutOptions& opts, std::ostream& out) {
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            ProcessLine(line, opts, out);
        }
    }
};

// ============================================================================
// 4. OPTION PARSER
// ============================================================================
class OptionParser {
public:
    static void DisplayHelp() {
        std::cout << R"(cut(1)                  CrossShell for UNIX Reference Manual                  cut(1)

    NAME
        cut - remove sections from each line of files

    SYNOPSIS
        cut OPTION... [FILE...]

    DESCRIPTION
        Print selected parts of lines from each FILE to standard output.
        With no FILE, or when FILE is -, read standard input.

    OPTIONS
        -b, --bytes LIST
            Select only these bytes.

        -c, --characters LIST
            Select only these characters.

        -d, --delimiter DELIM
            Use DELIM instead of TAB for field delimiter.

        -f, --fields LIST
            Select only these fields; also print any line that contains no
            delimiter character, unless the -s option is specified.

        -s, --only-delimited
            Do not print lines not containing delimiters.

        --complement
            Complement the set of selected bytes, characters, or fields.

        --output-delimiter STRING
            Use STRING as the output delimiter (default: input delimiter).

        --json, --csv, --table
            Output in JSON, CSV, or tabular format.

        --pipe COMMAND
            Send output directly through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        cut -d: -f1 /etc/passwd
            Output first field (usernames) separated by colon.

        cut -c1-10 file.txt
            Output characters 1 through 10 of each line.

        cut -d, -f2,4 --output-delimiter=TAB data.csv
            Extract columns 2 and 4 from CSV.

    CrossShell for UNIX                                                    cut(1)
)";
    }

    static void DisplayVersion() {
        std::cout << "cut (Extended Utilities) 2.0\n"
                  << "Copyright (C) 2026 Roberto J. Dohnert\n";
    }

    bool Parse(int argc, char* argv[], CutOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        std::string b_list, c_list, f_list;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-h") {
                DisplayHelp();
                exitEarly = true;
                return true;
            }
            if (arg == "--version") {
                DisplayVersion();
                exitEarly = true;
                return true;
            }
            if (arg == "--json") { opts.output_format = 1; continue; }
            if (arg == "--csv") { opts.output_format = 2; continue; }
            if (arg == "--table") { opts.output_format = 3; continue; }
            if (arg == "--pipe" && i + 1 < argc) { opts.pipe_command = argv[++i]; continue; }
            if (arg == "--complement") { opts.complement = true; continue; }

            if (arg.rfind("--output-delimiter=", 0) == 0) {
                opts.output_delimiter = arg.substr(19);
                continue;
            }
            if (arg == "--output-delimiter") {
                if (i + 1 >= argc) {
                    std::cerr << "cut: option requires an argument -- output-delimiter\n";
                    return false;
                }
                opts.output_delimiter = argv[++i];
                continue;
            }

            if (arg.rfind("--delimiter=", 0) == 0) {
                opts.delimiter = arg.substr(12);
                continue;
            }
            if (arg.rfind("--fields=", 0) == 0) {
                f_list = arg.substr(9);
                continue;
            }
            if (arg.rfind("--bytes=", 0) == 0) {
                b_list = arg.substr(8);
                continue;
            }
            if (arg.rfind("--characters=", 0) == 0) {
                c_list = arg.substr(13);
                continue;
            }

            if (arg.rfind("-", 0) == 0 && arg.size() > 1 && arg != "-") {
                char opt = arg[1];
                std::string val;

                if (arg.size() > 2) {
                    val = arg.substr(2);
                } else if (opt != 's') {
                    if (i + 1 < argc) {
                        val = argv[++i];
                    } else {
                        std::cerr << "cut: option requires an argument -- " << opt << "\n";
                        return false;
                    }
                }

                if (opt == 'b') b_list = val;
                else if (opt == 'c') c_list = val;
                else if (opt == 'f') f_list = val;
                else if (opt == 'd') {
                    if (val.empty()) {
                        std::cerr << "cut: bad delimiter\n";
                        return false;
                    }
                    opts.delimiter = val;
                }
                else if (opt == 's') opts.only_delimited = true;
                else {
                    std::cerr << "cut: unknown option -- " << opt << "\n";
                    return false;
                }
            } else {
                opts.files.push_back(arg);
            }
        }

        int modes = (b_list.empty() ? 0 : 1) + (c_list.empty() ? 0 : 1) + (f_list.empty() ? 0 : 1);
        if (modes != 1) {
            std::cerr << "cut: must specify only one of -b, -c, or -f\n";
            return false;
        }

        if (!b_list.empty()) {
            opts.mode = CutMode::BYTES;
            opts.list_str = b_list;
        } else if (!c_list.empty()) {
            opts.mode = CutMode::CHARACTERS;
            opts.list_str = c_list;
        } else {
            opts.mode = CutMode::FIELDS;
            opts.list_str = f_list;
        }

        if (!RangeParser::ParseList(opts.list_str, opts.selection)) {
            std::cerr << "cut: " << opts.list_str << ": invalid list value\n";
            return false;
        }

        return true;
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================
class CutApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]) {
        CutOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            OptionParser::DisplayHelp();
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

        std::ostringstream captured;
        std::streambuf* oldOutput = nullptr;
        if (opts.output_format || !opts.pipe_command.empty()) {
            oldOutput = std::cout.rdbuf(captured.rdbuf());
        }

        bool success = true;
        if (opts.files.empty()) {
            StreamProcessor::ProcessStream(std::cin, opts, std::cout);
        } else {
            for (const auto& file : opts.files) {
                if (file == "-") {
                    StreamProcessor::ProcessStream(std::cin, opts, std::cout);
                } else {
                    std::ifstream ifs(file);
                    if (!ifs.is_open()) {
                        std::cerr << "cut: " << file << ": No such file or directory\n";
                        success = false;
                        continue;
                    }
                    StreamProcessor::ProcessStream(ifs, opts, std::cout);
                }
            }
        }

        if (oldOutput) {
            std::cout.rdbuf(oldOutput);
            std::string data = captured.str();
            std::string text = (opts.output_format == 1) ? "{\"output\":\"" + data + "\"}\n"
                             : (opts.output_format == 2) ? "\"output\"\n\"" + data + "\"\n"
                             : "OUTPUT\n------\n" + data;
            if (!opts.pipe_command.empty()) {
                FILE* pipe = _popen(opts.pipe_command.c_str(), "w");
                if (!pipe) return 1;
                fwrite(text.data(), 1, text.size(), pipe);
                _pclose(pipe);
            } else {
                std::cout << text;
            }
        }

        return success ? 0 : 1;
    }
};

int main(int argc, char* argv[]) {
    CutApplication app;
    return app.Run(argc, argv);
}
