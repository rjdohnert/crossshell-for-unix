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

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

// ============================================================================
// 1. INPUT STREAM & DELIMITER PARSER
// ============================================================================

struct InputStream {
    bool is_stdin = false;
    std::ifstream file;
    bool eof = false;
};

class DelimiterParser {
public:
    static std::vector<std::string> Parse(const std::string& list) {
        std::vector<std::string> delims;
        if (list.empty()) {
            return delims;
        }
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i] == '\\') {
                if (i + 1 < list.size()) {
                    char next = list[i + 1];
                    switch (next) {
                        case 't': delims.push_back("\t"); break;
                        case 'n': delims.push_back("\n"); break;
                        case 'r': delims.push_back("\r"); break;
                        case 'f': delims.push_back("\f"); break;
                        case '\\': delims.push_back("\\"); break;
                        case '0': delims.push_back(""); break;
                        default: delims.push_back(std::string(1, next)); break;
                    }
                    ++i;
                } else {
                    delims.push_back("\\");
                }
            } else {
                delims.push_back(std::string(1, list[i]));
            }
        }
        return delims;
    }
};

class StreamReader {
public:
    static bool GetNextLine(InputStream& stream, std::string& line) {
        if (stream.eof) {
            line = "";
            return false;
        }
        std::istream& in = stream.is_stdin ? std::cin : stream.file;
        if (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return true;
        } else {
            stream.eof = true;
            line = "";
            return false;
        }
    }
};

// ============================================================================
// 2. PASTE ENGINE
// ============================================================================

class PasteEngine {
private:
    std::vector<std::string> m_delimiters;

public:
    explicit PasteEngine(std::vector<std::string> delimiters)
        : m_delimiters(std::move(delimiters)) {
        if (m_delimiters.empty()) {
            m_delimiters.push_back("");
        }
    }

    void ExecuteSerial(const std::vector<std::string>& files) const {
        for (const auto& file : files) {
            InputStream stream;
            if (file == "-") {
                stream.is_stdin = true;
            } else {
                stream.file.open(file, std::ios::binary);
            }

            std::string line;
            bool first = true;
            size_t delim_idx = 0;

            while (StreamReader::GetNextLine(stream, line)) {
                if (!first) {
                    std::cout << m_delimiters[delim_idx % m_delimiters.size()];
                    delim_idx++;
                }
                std::cout << line;
                first = false;
            }
            if (!first) {
                std::cout << "\n";
            }
        }
    }

    void ExecuteParallel(const std::vector<std::string>& files) const {
        std::vector<InputStream> streams;
        for (const auto& file : files) {
            InputStream stream;
            if (file == "-") {
                stream.is_stdin = true;
            } else {
                stream.file.open(file, std::ios::binary);
            }
            streams.push_back(std::move(stream));
        }

        size_t num_streams = streams.size();
        while (true) {
            std::vector<std::string> current_lines(num_streams);
            std::vector<bool> success_flags(num_streams);
            bool any_active = false;

            for (size_t i = 0; i < num_streams; ++i) {
                if (!streams[i].eof) {
                    any_active = true;
                    std::string line;
                    bool ok = StreamReader::GetNextLine(streams[i], line);
                    current_lines[i] = line;
                    success_flags[i] = ok;
                } else {
                    current_lines[i] = "";
                    success_flags[i] = false;
                }
            }

            if (!any_active) {
                break;
            }

            bool any_success = false;
            for (size_t i = 0; i < num_streams; ++i) {
                if (success_flags[i]) {
                    any_success = true;
                    break;
                }
            }
            if (!any_success) {
                break;
            }

            for (size_t i = 0; i < num_streams; ++i) {
                std::cout << current_lines[i];
                if (i < num_streams - 1) {
                    std::cout << m_delimiters[i % m_delimiters.size()];
                }
            }
            std::cout << "\n";
        }
    }
};

// ============================================================================
// 3. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

struct PasteOptions {
    std::string delim_list = "\t";
    bool serial = false;
    bool show_help = false;
    bool show_version = false;
    std::vector<std::string> files;
};

class OptionParser {
public:
    static void PrintUsage(const char* prog_name = "paste") {
        std::cout << R"(paste(1)                CrossShell for UNIX Reference Manual                  paste(1)

    NAME
        paste - merge lines of files

    SYNOPSIS
        paste [OPTIONS] [FILE...]

    DESCRIPTION
        paste writes lines consisting of the sequentially corresponding lines
        read from each FILE, separated by TABs, to standard output. With no
        FILE, or when FILE is '-', paste reads standard input.

    OPTIONS
        -d, --delimiters LIST
            Reuse characters from LIST instead of TABs. Escape sequences
            such as \n, \t, \r, \f, \0, and \\ are recognized.

        -s, --serial
            Paste one file at a time serially instead of in parallel.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version information.

    EXAMPLES
        paste file1.txt file2.txt
            Merge lines from file1 and file2 side-by-side with tabs.

        paste -d "," file1.csv file2.csv
            Merge corresponding lines using comma delimiter.

        paste -s -d "\t\n" names.txt
            Format items in pairs on each output line.

        cat list.txt | paste -d " - " - descriptions.txt
            Combine standard input with lines from descriptions.txt.

    CrossShell for UNIX                                                    paste(1)
)";
    }

    static void PrintVersion() {
        std::cout << "paste v1.0.0\n";
    }

    bool Parse(int argc, char* argv[], PasteOptions& opts) const {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--") {
                for (int j = i + 1; j < argc; ++j) {
                    opts.files.push_back(argv[j]);
                }
                break;
            } else if (arg == "-h" || arg == "--help") {
                opts.show_help = true;
                return true;
            } else if (arg == "--version" || arg == "-V") {
                opts.show_version = true;
                return true;
            } else if (arg == "-s" || arg == "--serial") {
                opts.serial = true;
            } else if (arg == "-d" || arg == "--delimiters") {
                if (i + 1 < argc) {
                    opts.delim_list = argv[++i];
                } else {
                    std::cerr << "paste: option requires an argument -- '" << arg << "'\n";
                    return false;
                }
            } else if (arg.rfind("-d", 0) == 0 && arg.length() > 2) {
                opts.delim_list = arg.substr(2);
            } else if (arg.rfind("--delimiters=", 0) == 0) {
                opts.delim_list = arg.substr(13);
            } else if (!arg.empty() && arg[0] == '-' && arg != "-") {
                // Compound short flags, e.g. -sd, -s
                bool valid = true;
                for (size_t c = 1; c < arg.length(); ++c) {
                    if (arg[c] == 's') {
                        opts.serial = true;
                    } else if (arg[c] == 'd') {
                        if (c + 1 < arg.length()) {
                            opts.delim_list = arg.substr(c + 1);
                            break;
                        } else if (i + 1 < argc) {
                            opts.delim_list = argv[++i];
                            break;
                        } else {
                            std::cerr << "paste: option requires an argument -- 'd'\n";
                            return false;
                        }
                    } else {
                        valid = false;
                        std::cerr << "paste: invalid option -- '" << arg[c] << "'\n";
                        break;
                    }
                }
                if (!valid) {
                    return false;
                }
            } else {
                opts.files.push_back(arg);
            }
        }
        if (opts.files.empty()) {
            opts.files.push_back("-");
        }
        return true;
    }
};

class PasteApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]) {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif

        PasteOptions opts;
        if (!m_parser.Parse(argc, argv, opts)) {
            OptionParser::PrintUsage(argc > 0 ? argv[0] : "paste");
            return 1;
        }

        if (opts.show_help) {
            OptionParser::PrintUsage(argc > 0 ? argv[0] : "paste");
            return 0;
        }

        if (opts.show_version) {
            OptionParser::PrintVersion();
            return 0;
        }

        std::vector<std::string> delims = DelimiterParser::Parse(opts.delim_list);
        PasteEngine engine(std::move(delims));

        if (opts.serial) {
            engine.ExecuteSerial(opts.files);
        } else {
            engine.ExecuteParallel(opts.files);
        }

        return 0;
    }
};

// ============================================================================
// 4. ENTRY POINT
// ============================================================================

int main(int argc, char* argv[]) {
    PasteApplication app;
    return app.Run(argc, argv);
}
