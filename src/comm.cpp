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
#include <string_view>
#include <vector>
#include <memory>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <optional>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace CommUtil {

    // Program Exit Codes
    enum class ExitCode : int {
        Success = 0,
        Failure = 1
    };

    // Configuration Settings
    struct Options {
        std::string file1;
        std::string file2;
        bool show_col1 = true;         // Unique to file1
        bool show_col2 = true;         // Unique to file2
        bool show_col3 = true;         // Common to both
        bool check_order = true;       // Check input sort order
        bool ignore_case = false;      // Case-insensitive comparisons
        bool show_total = false;       // Summary count of lines
        char delimiter = '\n';         // Line separator (default '\n', or '\0' for -z)
        std::string col_delimiter = "\t"; // Column separator (default TAB)
        bool show_help = false;
        bool show_version = false;
    };

    // =========================================================================
    // String Comparator Utility
    // =========================================================================
    class StringComparator {
    public:
        static int compare(std::string_view a, std::string_view b, bool ignore_case) {
            if (!ignore_case) {
                if (a < b) return -1;
                if (a > b) return 1;
                return 0;
            }

            auto it_a = a.begin();
            auto it_b = b.begin();

            while (it_a != a.end() && it_b != b.end()) {
                unsigned char ca = static_cast<unsigned char>(std::tolower(*it_a));
                unsigned char cb = static_cast<unsigned char>(std::tolower(*it_b));
                if (ca < cb) return -1;
                if (ca > cb) return 1;
                ++it_a;
                ++it_b;
            }

            if (a.length() < b.length()) return -1;
            if (a.length() > b.length()) return 1;
            return 0;
        }
    };

    // =========================================================================
    // Line Stream Abstraction (Supports Files, Stdin, and Custom Delimiters)
    // =========================================================================
    class ILineStream {
    public:
        virtual ~ILineStream() = default;
        virtual bool open() = 0;
        virtual bool get_next_line(std::string& out_line) = 0;
        virtual const std::string& get_name() const = 0;
        virtual uint64_t get_line_number() const = 0;
        virtual bool is_eof() const = 0;
    };

    class LineStream : public ILineStream {
    private:
        std::string name_;
        char line_delim_;
        std::istream* stream_ = nullptr;
        std::unique_ptr<std::ifstream> file_stream_;
        uint64_t line_number_ = 0;
        bool is_stdin_ = false;
        bool eof_ = false;

    public:
        LineStream(std::string name, char line_delim)
            : name_(std::move(name)), line_delim_(line_delim) {
            if (name_ == "-") {
                is_stdin_ = true;
            }
        }

        bool open() override {
            if (is_stdin_) {
#ifdef _WIN32
                // Enable binary stream on Windows for accurate byte-level piping
                if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
                    return false;
                }
#endif
                stream_ = &std::cin;
                return true;
            } else {
                file_stream_ = std::make_unique<std::ifstream>(name_, std::ios::binary);
                if (!file_stream_->is_open()) {
                    return false;
                }
                stream_ = file_stream_.get();
                return true;
            }
        }

        bool get_next_line(std::string& out_line) override {
            if (!stream_ || eof_) {
                return false;
            }

            if (std::getline(*stream_, out_line, line_delim_)) {
                // When line delimiter is newline '\n', trim trailing '\r' for CRLF compatibility
                if (line_delim_ == '\n' && !out_line.empty() && out_line.back() == '\r') {
                    out_line.pop_back();
                }
                line_number_++;
                return true;
            }

            eof_ = true;
            return false;
        }

        const std::string& get_name() const override { return name_; }
        uint64_t get_line_number() const override { return line_number_; }
        bool is_eof() const override { return eof_; }
    };

    // =========================================================================
    // Sort Order Monitor
    // =========================================================================
    class OrderValidator {
    private:
        std::string previous_line_;
        bool has_previous_ = false;
        bool order_error_found_ = false;
        bool ignore_case_;
        bool check_order_;
        std::string stream_name_;

    public:
        OrderValidator(std::string stream_name, bool check_order, bool ignore_case)
            : stream_name_(std::move(stream_name)),
              ignore_case_(ignore_case),
              check_order_(check_order) {}

        bool validate(const std::string& current_line, uint64_t line_number) {
            if (!check_order_) {
                return true;
            }

            if (has_previous_) {
                if (StringComparator::compare(previous_line_, current_line, ignore_case_) > 0) {
                    if (!order_error_found_) {
                        std::cerr << "comm: file '" << stream_name_
                                  << "' is not in sorted order on line " << line_number << "\n";
                        order_error_found_ = true;
                    }
                    return false;
                }
            }

            previous_line_ = current_line;
            has_previous_ = true;
            return true;
        }

        bool has_error() const { return order_error_found_; }
    };

    // =========================================================================
    // Column Output Formatter
    // =========================================================================
    class ColumnFormatter {
    private:
        const Options& opts_;
        std::string col1_prefix_;
        std::string col2_prefix_;
        std::string col3_prefix_;
        uint64_t count_col1_ = 0;
        uint64_t count_col2_ = 0;
        uint64_t count_col3_ = 0;

    public:
        explicit ColumnFormatter(const Options& opts) : opts_(opts) {
            // Calculate column prefixes based on active suppression flags
            // Col 1 never has leading tabs
            col1_prefix_ = "";

            // Col 2 prefix has delimiter only if Col 1 is visible
            col2_prefix_ = opts_.show_col1 ? opts_.col_delimiter : "";

            // Col 3 prefix has delimiters for each visible preceding column
            col3_prefix_ = (opts_.show_col1 ? opts_.col_delimiter : "") +
                           (opts_.show_col2 ? opts_.col_delimiter : "");
        }

        void print_col1(const std::string& line) {
            count_col1_++;
            if (opts_.show_col1) {
                std::cout << col1_prefix_ << line << opts_.delimiter;
            }
        }

        void print_col2(const std::string& line) {
            count_col2_++;
            if (opts_.show_col2) {
                std::cout << col2_prefix_ << line << opts_.delimiter;
            }
        }

        void print_col3(const std::string& line) {
            count_col3_++;
            if (opts_.show_col3) {
                std::cout << col3_prefix_ << line << opts_.delimiter;
            }
        }

        void print_totals() const {
            if (opts_.show_total) {
                std::cout << count_col1_ << opts_.col_delimiter
                          << count_col2_ << opts_.col_delimiter
                          << count_col3_ << opts_.col_delimiter
                          << "total" << opts_.delimiter;
            }
        }
    };

    // =========================================================================
    // Core Engine (Three-Way Merge Algorithm)
    // =========================================================================
    class CommEngine {
    private:
        Options opts_;
        std::unique_ptr<ILineStream> stream1_;
        std::unique_ptr<ILineStream> stream2_;
        ColumnFormatter formatter_;
        OrderValidator validator1_;
        OrderValidator validator2_;

    public:
        explicit CommEngine(Options opts)
            : opts_(opts),
              stream1_(std::make_unique<LineStream>(opts_.file1, opts_.delimiter)),
              stream2_(std::make_unique<LineStream>(opts_.file2, opts_.delimiter)),
              formatter_(opts_),
              validator1_(opts_.file1, opts_.check_order, opts_.ignore_case),
              validator2_(opts_.file2, opts_.check_order, opts_.ignore_case) {}

        ExitCode execute() {
            if (opts_.file1 == "-" && opts_.file2 == "-") {
                std::cerr << "comm: cannot compare standard input to itself\n";
                return ExitCode::Failure;
            }

            if (!stream1_->open()) {
                std::cerr << "comm: " << opts_.file1 << ": No such file or directory\n";
                return ExitCode::Failure;
            }
            if (!stream2_->open()) {
                std::cerr << "comm: " << opts_.file2 << ": No such file or directory\n";
                return ExitCode::Failure;
            }

            std::string line1, line2;
            bool has1 = stream1_->get_next_line(line1);
            bool has2 = stream2_->get_next_line(line2);

            bool order_ok = true;

            while (has1 && has2) {
                if (!validator1_.validate(line1, stream1_->get_line_number())) order_ok = false;
                if (!validator2_.validate(line2, stream2_->get_line_number())) order_ok = false;

                int comp = StringComparator::compare(line1, line2, opts_.ignore_case);

                if (comp < 0) {
                    formatter_.print_col1(line1);
                    has1 = stream1_->get_next_line(line1);
                } else if (comp > 0) {
                    formatter_.print_col2(line2);
                    has2 = stream2_->get_next_line(line2);
                } else {
                    formatter_.print_col3(line1);
                    has1 = stream1_->get_next_line(line1);
                    has2 = stream2_->get_next_line(line2);
                }
            }

            // Flush remaining lines from stream1
            while (has1) {
                if (!validator1_.validate(line1, stream1_->get_line_number())) order_ok = false;
                formatter_.print_col1(line1);
                has1 = stream1_->get_next_line(line1);
            }

            // Flush remaining lines from stream2
            while (has2) {
                if (!validator2_.validate(line2, stream2_->get_line_number())) order_ok = false;
                formatter_.print_col2(line2);
                has2 = stream2_->get_next_line(line2);
            }

            formatter_.print_totals();

            return (order_ok || !opts_.check_order) ? ExitCode::Success : ExitCode::Failure;
        }
    };

    // =========================================================================
    // Command Line Argument Parser
    // =========================================================================
    class ArgParser {
    public:
        static bool parse(int argc, char* argv[], Options& opts) {
            std::vector<std::string> positional;

            for (int i = 1; i < argc; ++i) {
                std::string arg = argv[i];

                if (arg == "--help" || arg == "-h" || arg == "-?") {
                    opts.show_help = true;
                    return true;
                } else if (arg == "--version" || arg == "-v") {
                    opts.show_version = true;
                    return true;
                } else if (arg == "--check-order") {
                    opts.check_order = true;
                } else if (arg == "--nocheck-order") {
                    opts.check_order = false;
                } else if (arg == "-i" || arg == "--ignore-case") {
                    opts.ignore_case = true;
                } else if (arg == "-z" || arg == "--zero-terminated") {
                    opts.delimiter = '\0';
                } else if (arg == "--total") {
                    opts.show_total = true;
                } else if (arg.rfind("--output-delimiter=", 0) == 0) {
                    opts.col_delimiter = arg.substr(19);
                } else if (arg == "--output-delimiter") {
                    if (i + 1 < argc) {
                        opts.col_delimiter = argv[++i];
                    } else {
                        std::cerr << "comm: option '--output-delimiter' requires an argument\n";
                        return false;
                    }
                } else if (arg.length() > 1 && arg[0] == '-' && arg != "-") {
                    // Handle clustered flags: -1, -2, -3, -12, -13, -23, -123, -i, -z
                    bool valid_cluster = true;
                    for (size_t j = 1; j < arg.length(); ++j) {
                        switch (arg[j]) {
                            case '1': opts.show_col1 = false; break;
                            case '2': opts.show_col2 = false; break;
                            case '3': opts.show_col3 = false; break;
                            case 'i': opts.ignore_case = true; break;
                            case 'z': opts.delimiter = '\0'; break;
                            default:
                                valid_cluster = false;
                                break;
                        }
                    }
                    if (!valid_cluster) {
                        std::cerr << "comm: unrecognized option '" << arg << "'\n"
                                  << "Try 'comm --help' for more information.\n";
                        return false;
                    }
                } else {
                    positional.push_back(arg);
                }
            }

            if (opts.show_help || opts.show_version) {
                return true;
            }

            if (positional.size() < 2) {
                std::cerr << "comm: missing operand" << (positional.empty() ? "" : " after '" + positional[0] + "'") << "\n"
                          << "Try 'comm --help' for more information.\n";
                return false;
            }

            if (positional.size() > 2) {
                std::cerr << "comm: extra operand '" << positional[2] << "'\n"
                          << "Try 'comm --help' for more information.\n";
                return false;
            }

            opts.file1 = positional[0];
            opts.file2 = positional[1];
            return true;
        }
    };

    // =========================================================================
    // Help & Information Subsystem
    // =========================================================================
    class HelpSystem {
    public:
        static void print_version() {
            std::cout << "comm version 2.9.0\n"
                      << "Copyright (C) 2026, Roberto J Dohnert\n";
        }

        static void print_help() {
            std::cout << R"(comm(1)                 CrossShell for UNIX Reference Manual                  comm(1)

    NAME
        comm - compare two sorted line streams

    SYNOPSIS
        comm [OPTIONS] FILE1 FILE2

    DESCRIPTION
        Merges two sorted inputs into columns for lines unique to FILE1, unique to
        FILE2, and common to both. One input may be '-', but both may not be.

    OPTIONS
        -1
            Suppress column 1 (lines unique to FILE1).

        -2
            Suppress column 2 (lines unique to FILE2).

        -3
            Suppress column 3 (lines that appear in both files).

        --check-order
            Check that the input is correctly sorted (default).

        --nocheck-order
            Disable sorted-order checking.

        --output-delimiter STRING
            Set the column delimiter (default is TAB).

        --total
            Append column counts and summary total at the end.

        -i, --ignore-case
            Compare lines case-insensitively.

        -z, --zero-terminated
            Use NUL line delimiters instead of newlines.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version and license information.

    EXAMPLES
        comm file1.txt file2.txt
            Produce standard three-column comparison.

        comm -12 file1.txt file2.txt
            Print only lines common to both files.

        comm -23 file1.txt file2.txt
            Print lines unique to file1.

        comm --output-delimiter=, file1.txt file2.txt
            Use comma delimiter between columns.

    CrossShell for UNIX                                                    comm(1)
)";
        }
    };

    // =========================================================================
    // Main Application Coordinator
    // =========================================================================
    class CommApp {
    public:
        static int run(int argc, char* argv[]) {
            std::ios_base::sync_with_stdio(false);
            std::cin.tie(nullptr);

            Options opts;
            if (!ArgParser::parse(argc, argv, opts)) {
                return static_cast<int>(ExitCode::Failure);
            }

            if (opts.show_help) {
                HelpSystem::print_help();
                return static_cast<int>(ExitCode::Success);
            }

            if (opts.show_version) {
                HelpSystem::print_version();
                return static_cast<int>(ExitCode::Success);
            }

            CommEngine engine(opts);
            return static_cast<int>(engine.execute());
        }
    };
}

int main(int argc, char* argv[]) {
    return CommUtil::CommApp::run(argc, argv);
}