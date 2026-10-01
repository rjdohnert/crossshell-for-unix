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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
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
#include <iomanip>
#include <cstdio>
#include <streambuf>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

// ============================================================================
// 1. DATA MODELS & PIPE STREAMING
// ============================================================================

struct Counters {
    long long lines = 0;
    long long words = 0;
    long long bytes = 0;
    long long chars = 0;
    long long max_line_length = 0;
};

enum class OutputFormat { Human, Json, Csv, Table };

class PipeBuffer : public std::streambuf {
    FILE* file_;
    char buffer_[4096];
public:
    explicit PipeBuffer(FILE* file) : file_(file) { setp(buffer_, buffer_ + sizeof(buffer_)); }
    int_type overflow(int_type ch) override {
        if (ch != traits_type::eof()) {
            *pptr() = static_cast<char>(ch);
            pbump(1);
        }
        return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
    }
    int sync() override {
        auto n = pptr() - pbase();
        if (n && std::fwrite(pbase(), 1, static_cast<size_t>(n), file_) != static_cast<size_t>(n)) return -1;
        setp(buffer_, buffer_ + sizeof(buffer_));
        return std::fflush(file_) == 0 ? 0 : -1;
    }
};

class PipeSession {
    std::streambuf* old_;
    FILE* file_ = nullptr;
    PipeBuffer* buffer_ = nullptr;
public:
    explicit PipeSession(const std::string& command) : old_(std::cout.rdbuf()) {
        if (!command.empty() && (file_ = _popen(command.c_str(), "w"))) {
            buffer_ = new PipeBuffer(file_);
            std::cout.rdbuf(buffer_);
        }
    }
    ~PipeSession() {
        std::cout.flush();
        std::cout.rdbuf(old_);
        delete buffer_;
        if (file_) _pclose(file_);
    }
};

// ============================================================================
// 2. OUTPUT FORMATTER
// ============================================================================

struct WcOptions {
    bool opt_bytes = false;
    bool opt_chars = false;
    bool opt_lines = false;
    bool opt_words = false;
    bool opt_max_len = false;
    bool implicit_stdin = false;
    OutputFormat format = OutputFormat::Human;
    std::string pipe_command;
    std::vector<std::string> files;
};

class OutputFormatter {
public:
    static void PrintHeader(OutputFormat format) {
        if (format == OutputFormat::Csv) {
            std::cout << "\"file\",lines,words,chars,bytes,max_line_length\n";
        } else if (format == OutputFormat::Table) {
            std::cout << "FILE\tLINES\tWORDS\tCHARS\tBYTES\tMAX_LINE_LENGTH\n";
        }
    }

    static void PrintCounts(const Counters& c, const WcOptions& opts, const std::string& name) {
        if (opts.format == OutputFormat::Json) {
            std::cout << "{\"file\":\"" << name << "\",\"lines\":" << c.lines << ",\"words\":" << c.words
                      << ",\"chars\":" << c.chars << ",\"bytes\":" << c.bytes << ",\"max_line_length\":" << c.max_line_length << "}\n";
            return;
        }
        if (opts.format == OutputFormat::Csv) {
            std::cout << '"' << name << "\"," << c.lines << ',' << c.words << ',' << c.chars << ',' << c.bytes << ',' << c.max_line_length << "\n";
            return;
        }
        if (opts.format == OutputFormat::Table) {
            std::cout << name << "\t" << c.lines << "\t" << c.words << "\t" << c.chars << "\t" << c.bytes << "\t" << c.max_line_length << "\n";
            return;
        }
        if (opts.opt_lines) {
            std::cout << std::setw(8) << c.lines;
        }
        if (opts.opt_words) {
            std::cout << std::setw(8) << c.words;
        }
        if (opts.opt_chars) {
            std::cout << std::setw(8) << c.chars;
        }
        if (opts.opt_bytes) {
            std::cout << std::setw(8) << c.bytes;
        }
        if (opts.opt_max_len) {
            std::cout << std::setw(8) << c.max_line_length;
        }
        if (!name.empty()) {
            std::cout << " " << name;
        }
        std::cout << "\n";
    }
};

// ============================================================================
// 3. TEXT METRICS SCANNER
// ============================================================================

class TextMetricsScanner {
public:
    static void ScanStream(std::istream& in, Counters& c) {
        char buffer[16384];
        bool in_word = false;
        long long current_line_len = 0;

        int utf8_expected_continuations = 0;
        bool utf8_pending_starter = false;

        auto advance_display_width = [&current_line_len](unsigned char ch) {
            if (ch == '\t') {
                long long tab_advance = 8 - (current_line_len % 8);
                current_line_len += tab_advance;
                return;
            }
            if (ch < 0x20 || ch == 0x7F || ch == '\r' || ch == '\n') {
                return;
            }
            current_line_len++;
        };

        auto count_completed_codepoint = [&c, &current_line_len]() {
            c.chars++;
            current_line_len++;
        };

        while (in.read(buffer, sizeof(buffer)) || in.gcount() > 0) {
            std::streamsize bytes_read = in.gcount();
            c.bytes += bytes_read;

            for (std::streamsize i = 0; i < bytes_read; ++i) {
                unsigned char ch = static_cast<unsigned char>(buffer[i]);

                bool is_space = (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f');
                if (is_space) {
                    in_word = false;
                } else if (!in_word) {
                    in_word = true;
                    c.words++;
                }

                if (ch == '\n') {
                    if (utf8_pending_starter) {
                        utf8_expected_continuations = 0;
                        utf8_pending_starter = false;
                        count_completed_codepoint();
                    }
                    c.lines++;
                    if (current_line_len > c.max_line_length) {
                        c.max_line_length = current_line_len;
                    }
                    current_line_len = 0;
                }

                if (utf8_expected_continuations > 0) {
                    if ((ch & 0xC0) == 0x80) {
                        utf8_expected_continuations--;
                        if (utf8_expected_continuations == 0) {
                            utf8_pending_starter = false;
                            count_completed_codepoint();
                        }
                        continue;
                    } else {
                        utf8_expected_continuations = 0;
                        if (utf8_pending_starter) {
                            utf8_pending_starter = false;
                            count_completed_codepoint();
                        }
                    }
                }

                if (ch < 0x80) {
                    c.chars++;
                    if (ch != '\n') {
                        advance_display_width(ch);
                    }
                } else if (ch >= 0xC2 && ch <= 0xDF) {
                    utf8_expected_continuations = 1;
                    utf8_pending_starter = true;
                } else if (ch >= 0xE0 && ch <= 0xEF) {
                    utf8_expected_continuations = 2;
                    utf8_pending_starter = true;
                } else if (ch >= 0xF0 && ch <= 0xF4) {
                    utf8_expected_continuations = 3;
                    utf8_pending_starter = true;
                } else {
                    c.chars++;
                    if (ch != '\n') {
                        advance_display_width(ch);
                    }
                }
            }
        }

        if (utf8_pending_starter) {
            count_completed_codepoint();
        }

        if (current_line_len > c.max_line_length) {
            c.max_line_length = current_line_len;
        }
    }
};

// ============================================================================
// 4. OPTION PARSER & APPLICATION RUNNER
// ============================================================================

class OptionParser {
public:
    static void PrintUsage() {
        std::cout << R"(wc(1)                   CrossShell for UNIX Reference Manual                 wc(1)

    NAME
        wc - print newline, word, and byte counts for each file

    SYNOPSIS
        wc [OPTIONS] [FILE...]

    DESCRIPTION
        Print newline, word, and byte counts for each FILE, and a total line if
        more than one FILE is specified. A word is a non-zero-length sequence of
        printable characters delimited by white space.
        With no FILE, or when FILE is -, read standard input.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -c, --bytes
            Print the byte counts.

        -m, --chars
            Print the character counts.

        -l, --lines
            Print the newline counts.

        -w, --words
            Print the word counts.

        -L, --max-line-length
            Print the maximum display width.

        --json, --csv, --table
            Select output format (JSON, CSV, or Table).

        --pipe COMMAND
            Send output through pipeline COMMAND.

        -h, --help
            Display this reference manual and exit.

        -V, -v, --version
            Display version information and exit.

    EXAMPLES
        wc file.txt
            Print line, word, and byte counts for file.txt.

        wc -l file1.txt file2.txt
            Print line counts for both files and total.

        dir | wc -l
            Count lines in directory listing.

    CrossShell for UNIX                                                      wc(1)
)";
    }

    static void PrintVersion() {
        std::cout << "wc 1.0.0\n";
    }

    bool Parse(int argc, char* argv[], WcOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--") {
                for (int j = i + 1; j < argc; ++j) {
                    opts.files.push_back(argv[j]);
                }
                break;
            } else if (arg == "-") {
                opts.files.push_back("-");
            } else if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") {
                PrintUsage();
                exitEarly = true;
                return true;
            } else if (arg == "-v" || arg == "-V" || arg == "--version") {
                PrintVersion();
                exitEarly = true;
                return true;
            } else if (arg == "--bytes") {
                opts.opt_bytes = true;
            } else if (arg == "--chars") {
                opts.opt_chars = true;
            } else if (arg == "--lines") {
                opts.opt_lines = true;
            } else if (arg == "--words") {
                opts.opt_words = true;
            } else if (arg == "--max-line-length") {
                opts.opt_max_len = true;
            } else if (arg == "--json") {
                opts.format = OutputFormat::Json;
            } else if (arg == "--csv") {
                opts.format = OutputFormat::Csv;
            } else if (arg == "--table") {
                opts.format = OutputFormat::Table;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipe_command = argv[++i];
            } else if (arg.rfind("--", 0) == 0) {
                std::cerr << "wc: unknown option -- " << arg.substr(2) << "\n";
                PrintUsage();
                return false;
            } else if (arg[0] == '-' && arg.size() > 1) {
                std::string opt_str = arg.substr(1);
                for (size_t j = 0; j < opt_str.size(); ++j) {
                    char opt = opt_str[j];
                    if (opt == 'c') opts.opt_bytes = true;
                    else if (opt == 'm') opts.opt_chars = true;
                    else if (opt == 'l') opts.opt_lines = true;
                    else if (opt == 'w') opts.opt_words = true;
                    else if (opt == 'L') opts.opt_max_len = true;
                    else if (opt == 'h' || opt == '?') {
                        PrintUsage();
                        exitEarly = true;
                        return true;
                    } else if (opt == 'V' || opt == 'v') {
                        PrintVersion();
                        exitEarly = true;
                        return true;
                    } else {
                        std::cerr << "wc: unknown option -- " << opt << "\n";
                        PrintUsage();
                        return false;
                    }
                }
            } else {
                opts.files.push_back(arg);
            }
        }

        bool any_flag = (opts.opt_bytes || opts.opt_chars || opts.opt_lines || opts.opt_words || opts.opt_max_len);
        if (!any_flag) {
            opts.opt_lines = true;
            opts.opt_words = true;
            opts.opt_bytes = true;
        }

        if (opts.files.empty()) {
            opts.implicit_stdin = true;
            opts.files.push_back("-");
        }

        return true;
    }
};

class WcApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]) {
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(NULL);

#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif

        WcOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

        PipeSession pipe_session(opts.pipe_command);
        OutputFormatter::PrintHeader(opts.format);

        Counters total;
        bool success = true;
        int processed_inputs = 0;

        for (const auto& file : opts.files) {
            Counters file_c;
            if (file == "-") {
                TextMetricsScanner::ScanStream(std::cin, file_c);
                OutputFormatter::PrintCounts(file_c, opts, opts.implicit_stdin ? "" : "-");
                processed_inputs++;
            } else {
                std::ifstream infile(file, std::ios_base::in | std::ios_base::binary);
                if (!infile.is_open()) {
                    std::cerr << "wc: " << file << ": No such file or directory\n";
                    success = false;
                    continue;
                }
                TextMetricsScanner::ScanStream(infile, file_c);
                OutputFormatter::PrintCounts(file_c, opts, file);
                processed_inputs++;
            }

            total.lines += file_c.lines;
            total.words += file_c.words;
            total.chars += file_c.chars;
            total.bytes += file_c.bytes;
            if (file_c.max_line_length > total.max_line_length) {
                total.max_line_length = file_c.max_line_length;
            }
        }

        if (processed_inputs > 1) {
            OutputFormatter::PrintCounts(total, opts, "total");
        }

        return success ? 0 : 1;
    }
};

int main(int argc, char* argv[]) {
    WcApplication app;
    return app.Run(argc, argv);
}
