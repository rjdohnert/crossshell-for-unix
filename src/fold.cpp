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
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <memory>
#include <cstdlib>
#include <cctype>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace FoldUtility {

    // =========================================================================
    // Configuration & Settings
    // =========================================================================
    struct Options {
        size_t width = 80;                     // Default wrap width
        bool count_bytes = false;             // -b, --bytes
        bool break_at_spaces = false;         // -s, --spaces
        std::vector<std::string> files;       // Files to process (empty or "-" means stdin)
        bool show_help = false;
        bool show_version = false;
        int output_format = 0;
        std::string pipe_command;
    };

    // =========================================================================
    // Command-Line Argument Parser
    // =========================================================================
    class ArgumentParser {
    public:
        static Options parse(int argc, char* argv[]) {
            Options options;

            for (int i = 1; i < argc; ++i) {
                std::string_view arg = argv[i];

                if (arg == "-h" || arg == "--help" || arg == "/?") {
                    options.show_help = true;
                    return options;
                }
                if (arg == "-v" || arg == "-V" || arg == "--version") {
                    options.show_version = true;
                    return options;
                }
                if (arg == "--json") { options.output_format = 1; continue; }
                if (arg == "--csv") { options.output_format = 2; continue; }
                if (arg == "--table") { options.output_format = 3; continue; }
                if (arg == "--pipe" && i + 1 < argc) { options.pipe_command = argv[++i]; continue; }
                if (arg == "-b" || arg == "--bytes") {
                    options.count_bytes = true;
                }
                else if (arg == "-s" || arg == "--spaces") {
                    options.break_at_spaces = true;
                }
                else if (arg == "-w" || arg == "--width") {
                    if (i + 1 < argc) {
                        options.width = parse_width(argv[++i]);
                    } else {
                        throw std::runtime_error("Option requires an argument: " + std::string(arg));
                    }
                }
                else if (arg.rfind("--width=", 0) == 0) {
                    options.width = parse_width(arg.substr(8));
                }
                else if (arg.rfind("-w", 0) == 0 && arg.size() > 2) {
                    options.width = parse_width(arg.substr(2));
                }
                else if (arg.size() > 1 && arg[0] == '-' && std::isdigit(static_cast<unsigned char>(arg[1]))) {
                    // Support legacy syntax like "-80"
                    options.width = parse_width(arg.substr(1));
                }
                else if (arg.size() > 1 && arg[0] == '-' && arg[1] != '-') {
                    // Parse bundled short flags (e.g. -sbw 50)
                    for (size_t j = 1; j < arg.size(); ++j) {
                        char flag = arg[j];
                        if (flag == 'b') {
                            options.count_bytes = true;
                        } else if (flag == 's') {
                            options.break_at_spaces = true;
                        } else if (flag == 'w') {
                            if (j + 1 < arg.size()) {
                                options.width = parse_width(arg.substr(j + 1));
                                break;
                            } else if (i + 1 < argc) {
                                options.width = parse_width(argv[++i]);
                                break;
                            } else {
                                throw std::runtime_error("Option -w requires an argument.");
                            }
                        } else {
                            throw std::runtime_error(std::string("Unknown flag: -") + flag);
                        }
                    }
                }
                else if (arg == "--") {
                    // Treat remainder as filenames
                    for (++i; i < argc; ++i) {
                        options.files.emplace_back(argv[i]);
                    }
                    break;
                }
                else {
                    options.files.emplace_back(arg);
                }
            }

            return options;
        }

        static void print_help(std::ostream& out) {
            out << R"(fold(1)             CrossShell for UNIX Reference Manual                 fold(1)

    NAME
        fold - wrap each input line to fit in specified width

    SYNOPSIS
        fold [OPTIONS] [FILE...]

    DESCRIPTION
        fold wraps input lines in each FILE (or standard input if no file is specified
        or when FILE is '-'), writing to standard output. It integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -b, --bytes
            Count bytes rather than display column positions.

        -s, --spaces
            Break lines at word boundaries (spaces/tabs) within the line width.

        -w, --width WIDTH
            Use specified column or byte width instead of the default 80.

        --json
            Emit folded lines as JSON records.

        --csv
            Emit folded lines as CSV records.

        --table
            Emit folded lines in a table format.

        --pipe COMMAND
            Send formatted output through the specified pipe command.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        fold -w 60 document.txt
            Wrap lines in document.txt to 60 columns.

        fold -s -w 72 long_text.txt > formatted.txt
            Wrap at 72 columns breaking only at spaces.

        type binary_dump.txt | fold -b -w 64
            Wrap byte-wise at 64 bytes per line.

    CrossShell for UNIX                                                    fold(1)
)";
        }

        static void print_version(std::ostream& out) {
            out << "fold 6.0.2\n"
                << "Copyright (C) 2026, Roberto J Dohnert.\n";
        }

    private:
        static size_t parse_width(std::string_view str) {
            try {
                size_t pos = 0;
                unsigned long val = std::stoul(std::string(str), &pos);
                if (pos != str.size() || val == 0) {
                    throw std::invalid_argument("");
                }
                return static_cast<size_t>(val);
            } catch (...) {
                throw std::runtime_error("Invalid width specification: '" + std::string(str) + "'. Width must be a positive integer.");
            }
        }
    };

    // =========================================================================
    // Text Folder Engine (Stream Processor)
    // =========================================================================
    class TextFolder {
    public:
        explicit TextFolder(const Options& opts) : m_opts(opts) {}

        void process_stream(std::istream& in, std::ostream& out) {
            std::string line_buffer;
            size_t current_col = 0;
            std::optional<size_t> last_space_index;

            char ch;
            while (in.get(ch)) {
                // Line breaks reset state and flush current buffer
                if (ch == '\n') {
                    out << line_buffer << '\n';
                    line_buffer.clear();
                    current_col = 0;
                    last_space_index.reset();
                    continue;
                }

                size_t char_width = calculate_char_width(ch, current_col);

                // Check if adding this character exceeds target width
                if (current_col + char_width > m_opts.width && !line_buffer.empty()) {
                    if (m_opts.break_at_spaces && last_space_index.has_value()) {
                        // Word boundary wrap: flush up to the last seen blank character
                        size_t split_pos = last_space_index.value();
                        out << line_buffer.substr(0, split_pos + 1) << '\n';

                        // Retain leftovers after the space for the next line
                        std::string remainder = line_buffer.substr(split_pos + 1);
                        line_buffer = remainder;
                        current_col = recalculate_width(remainder);
                        last_space_index.reset();
                        
                        // Recheck any spaces inside the carried over remainder
                        for (size_t i = 0; i < line_buffer.size(); ++i) {
                            if (is_blank(line_buffer[i])) {
                                last_space_index = i;
                            }
                        }

                        char_width = calculate_char_width(ch, current_col);
                    } else {
                        // Hard wrap
                        out << line_buffer << '\n';
                        line_buffer.clear();
                        current_col = 0;
                        last_space_index.reset();
                        char_width = calculate_char_width(ch, current_col);
                    }
                }

                // Update column track
                if (m_opts.count_bytes) {
                    current_col += 1;
                } else {
                    if (ch == '\b') {
                        current_col = (current_col > 0) ? current_col - 1 : 0;
                    } else if (ch == '\r') {
                        current_col = 0;
                    } else if (ch == '\t') {
                        current_col = next_tab_stop(current_col);
                    } else {
                        current_col += 1;
                    }
                }

                line_buffer.push_back(ch);

                if (is_blank(ch)) {
                    last_space_index = line_buffer.size() - 1;
                }
            }

            // Flush remaining buffer at EOF
            if (!line_buffer.empty()) {
                out << line_buffer;
            }
        }

    private:
        const Options& m_opts;

        static bool is_blank(char ch) noexcept {
            return ch == ' ' || ch == '\t';
        }

        static size_t next_tab_stop(size_t col) noexcept {
            return (col / 8 + 1) * 8;
        }

        size_t calculate_char_width(char ch, size_t col) const noexcept {
            if (m_opts.count_bytes) return 1;
            if (ch == '\t') return next_tab_stop(col) - col;
            if (ch == '\b' || ch == '\r') return 0;
            return 1;
        }

        size_t recalculate_width(std::string_view sv) const noexcept {
            if (m_opts.count_bytes) return sv.size();
            size_t col = 0;
            for (char ch : sv) {
                if (ch == '\t') col = next_tab_stop(col);
                else if (ch == '\b') col = (col > 0) ? col - 1 : 0;
                else if (ch == '\r') col = 0;
                else col += 1;
            }
            return col;
        }
    };

    // =========================================================================
    // Application Controller
    // =========================================================================
    class FoldApp {
    public:
        static int run(int argc, char* argv[]) {
            // Enable binary transparency on standard Windows I/O handles
            #ifdef _WIN32
            _setmode(_fileno(stdin), _O_BINARY);
            _setmode(_fileno(stdout), _O_BINARY);
            #endif

            std::ios_base::sync_with_stdio(false);
            std::cin.tie(nullptr);

            try {
                Options options = ArgumentParser::parse(argc, argv);

                if (options.show_help) {
                    ArgumentParser::print_help(std::cout);
                    return EXIT_SUCCESS;
                }

                if (options.show_version) {
                    ArgumentParser::print_version(std::cout);
                    return EXIT_SUCCESS;
                }

                TextFolder folder(options);
                std::ostringstream captured;
                std::ostream& output = (options.output_format != 0 || !options.pipe_command.empty()) ? static_cast<std::ostream&>(captured) : std::cout;

                // Pipe handling: If no input files given or "-" specified, process stdin
                if (options.files.empty()) {
                    folder.process_stream(std::cin, output);
                    return EXIT_SUCCESS;
                }

                int exit_code = EXIT_SUCCESS;
                for (const auto& file_path : options.files) {
                    if (file_path == "-") {
                        folder.process_stream(std::cin, output);
                    } else {
                        std::ifstream infile(file_path, std::ios::binary);
                        if (!infile.is_open()) {
                            std::cerr << "fold: cannot open '" << file_path << "': No such file or directory\n";
                            exit_code = EXIT_FAILURE;
                            continue;
                        }
                        folder.process_stream(infile, output);
                    }
                }
                if (options.output_format != 0 || !options.pipe_command.empty()) {
                    std::istringstream lines(captured.str());
                    std::ostringstream formatted;
                    if (options.output_format == 1) {
                        formatted << "[";
                        std::string line; bool first = true;
                        while (std::getline(lines, line)) { if (!first) formatted << ","; first = false; formatted << "{\"line\":\"" << line << "\"}"; }
                        formatted << "]\n";
                    } else if (options.output_format == 2) {
                        formatted << "line\n" << captured.str();
                    } else {
                        formatted << "LINE\n----\n" << captured.str();
                    }
                    std::string text = formatted.str();
                    if (!options.pipe_command.empty()) {
                        FILE* pipe = _popen(options.pipe_command.c_str(), "w");
                        if (!pipe) return EXIT_FAILURE;
                        fwrite(text.data(), 1, text.size(), pipe);
                        _pclose(pipe);
                    } else {
                        std::cout << text;
                    }
                }
                return exit_code;

            } catch (const std::exception& ex) {
                std::cerr << "fold: error: " << ex.what() << "\n";
                std::cerr << "Try 'fold --help' for more information.\n";
                return EXIT_FAILURE;
            }
        }
    };

} // namespace FoldUtility

int main(int argc, char* argv[]) {
    return FoldUtility::FoldApp::run(argc, argv);
}