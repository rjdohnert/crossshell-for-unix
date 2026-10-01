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
#include <cctype>
#include <algorithm>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace CmpUtil {

    // Program exit codes matching UNIX cmp standard
    enum class ExitCode : int {
        Identical = 0,
        Differ    = 1,
        Error     = 2
    };

    // Configuration structure
    struct Options {
        std::string file1;
        std::string file2;
        uint64_t skip1 = 0;
        uint64_t skip2 = 0;
        uint64_t limit_bytes = UINT64_MAX;
        bool verbose_all = false;    // -l: Output byte numbers and differing byte values
        bool print_bytes = false;    // -b: Print differing bytes (octal / chars)
        bool silent = false;         // -s, -q: Suppress output, return status only
        bool show_help = false;
        bool show_version = false;
    };

    // =========================================================================
    // IByteStream & Stream Implementations (Abstraction Layer for File / Stdin)
    // =========================================================================
    class IByteStream {
    public:
        virtual ~IByteStream() = default;
        virtual bool open() = 0;
        virtual bool read_byte(uint8_t& byte) = 0;
        virtual void skip(uint64_t count) = 0;
        virtual bool is_eof() const = 0;
        virtual const std::string& get_name() const = 0;
        virtual bool has_error() const = 0;
    };

    class ChunkedStream : public IByteStream {
    private:
        static constexpr size_t BUFFER_SIZE = 64 * 1024; // 64 KB buffer
        std::string name_;
        std::istream* stream_ = nullptr;
        std::unique_ptr<std::ifstream> file_stream_;
        std::vector<uint8_t> buffer_;
        size_t buf_pos_ = 0;
        size_t buf_len_ = 0;
        bool is_stdin_ = false;
        bool eof_reached_ = false;
        bool error_ = false;

    public:
        explicit ChunkedStream(std::string name)
            : name_(std::move(name)), buffer_(BUFFER_SIZE) {
            if (name_ == "-") {
                is_stdin_ = true;
            }
        }

        bool open() override {
            if (is_stdin_) {
#ifdef _WIN32
                // Force stdin to binary mode on Windows to avoid CRLF translations & EOF truncations
                if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
                    error_ = true;
                    return false;
                }
#endif
                stream_ = &std::cin;
                return true;
            } else {
                file_stream_ = std::make_unique<std::ifstream>(name_, std::ios::binary);
                if (!file_stream_->is_open()) {
                    error_ = true;
                    return false;
                }
                stream_ = file_stream_.get();
                return true;
            }
        }

        bool read_byte(uint8_t& byte) override {
            if (buf_pos_ >= buf_len_) {
                if (eof_reached_ || !refill_buffer()) {
                    return false;
                }
            }
            byte = buffer_[buf_pos_++];
            return true;
        }

        void skip(uint64_t count) override {
            uint8_t dummy;
            while (count > 0 && read_byte(dummy)) {
                --count;
            }
        }

        bool is_eof() const override {
            return eof_reached_ && (buf_pos_ >= buf_len_);
        }

        const std::string& get_name() const override {
            return name_;
        }

        bool has_error() const override {
            return error_;
        }

    private:
        bool refill_buffer() {
            if (!stream_ || !(*stream_)) {
                eof_reached_ = true;
                return false;
            }

            stream_->read(reinterpret_cast<char*>(buffer_.data()), BUFFER_SIZE);
            std::streamsize bytes_read = stream_->gcount();

            if (bytes_read <= 0) {
                eof_reached_ = true;
                return false;
            }

            buf_pos_ = 0;
            buf_len_ = static_cast<size_t>(bytes_read);
            return true;
        }
    };

    // =========================================================================
    // Format Utility
    // =========================================================================
    class Formatter {
    public:
        static std::string byte_representation(uint8_t b) {
            std::ostringstream oss;
            if (b >= 32 && b <= 126) {
                // Printable character
                oss << std::setw(3) << std::oct << static_cast<int>(b) << " " << static_cast<char>(b);
            } else if (b == '\b') {
                oss << "\\b";
            } else if (b == '\f') {
                oss << "\\f";
            } else if (b == '\n') {
                oss << "\\n";
            } else if (b == '\r') {
                oss << "\\r";
            } else if (b == '\t') {
                oss << "\\t";
            } else if (b == '\v') {
                oss << "\\v";
            } else {
                // Non-printable control char (e.g. ^A) or raw octal
                if (b < 32) {
                    oss << "^" << static_cast<char>(b + '@');
                } else {
                    oss << std::setw(3) << std::oct << static_cast<int>(b);
                }
            }
            return oss.str();
        }
    };

    // =========================================================================
    // Comparison Engine (Core Logic)
    // =========================================================================
    class FileComparator {
    private:
        Options opts_;
        std::unique_ptr<IByteStream> stream1_;
        std::unique_ptr<IByteStream> stream2_;

    public:
        explicit FileComparator(Options opts)
            : opts_(std::move(opts)),
              stream1_(std::make_unique<ChunkedStream>(opts_.file1)),
              stream2_(std::make_unique<ChunkedStream>(opts_.file2)) {}

        ExitCode run() {
            if (!stream1_->open()) {
                std::cerr << "cmp: " << opts_.file1 << ": No such file or directory or access denied\n";
                return ExitCode::Error;
            }
            if (!stream2_->open()) {
                std::cerr << "cmp: " << opts_.file2 << ": No such file or directory or access denied\n";
                return ExitCode::Error;
            }

            // Apply offsets
            stream1_->skip(opts_.skip1);
            stream2_->skip(opts_.skip2);

            uint64_t byte_count = 0;
            uint64_t line_count = 1;
            bool files_differ = false;

            while (byte_count < opts_.limit_bytes) {
                uint8_t b1 = 0, b2 = 0;
                bool read1 = stream1_->read_byte(b1);
                bool read2 = stream2_->read_byte(b2);

                if (!read1 || !read2) {
                    if (read1 != read2) {
                        files_differ = true;
                        if (!opts_.silent) {
                            const std::string& short_file = (!read1) ? opts_.file1 : opts_.file2;
                            std::cerr << "cmp: EOF on " << short_file;
                            if (byte_count > 0) {
                                std::cerr << " after byte " << byte_count << ", line " << line_count;
                            } else {
                                std::cerr << " which is empty";
                            }
                            std::cerr << "\n";
                        }
                    }
                    break;
                }

                byte_count++;

                if (b1 != b2) {
                    files_differ = true;
                    if (opts_.silent) {
                        return ExitCode::Differ;
                    }

                    if (opts_.verbose_all) {
                        // Print decimal byte offset and both bytes in octal
                        std::cout << std::dec << std::setw(6) << byte_count << " "
                                  << std::oct << std::setw(3) << static_cast<int>(b1) << " "
                                  << std::oct << std::setw(3) << static_cast<int>(b2) << "\n";
                    } else {
                        // Standard default single difference format
                        std::cout << opts_.file1 << " " << opts_.file2 << " differ: byte " 
                                  << std::dec << byte_count << ", line " << line_count;
                        
                        if (opts_.print_bytes) {
                            std::cout << " is " << std::oct << static_cast<int>(b1) << " " << Formatter::byte_representation(b1)
                                      << " " << std::oct << static_cast<int>(b2) << " " << Formatter::byte_representation(b2);
                        }
                        std::cout << "\n";
                        return ExitCode::Differ;
                    }
                }

                if (b1 == '\n') {
                    line_count++;
                }
            }

            return files_differ ? ExitCode::Differ : ExitCode::Identical;
        }
    };

    // =========================================================================
    // Command Line Argument Parser
    // =========================================================================
    class ArgParser {
    public:
        static uint64_t parse_size(const std::string& str) {
            uint64_t multiplier = 1;
            std::string num_part = str;

            if (!str.empty()) {
                char suffix = static_cast<char>(std::tolower(str.back()));
                if (suffix == 'k') { multiplier = 1024ULL; num_part.pop_back(); }
                else if (suffix == 'm') { multiplier = 1024ULL * 1024ULL; num_part.pop_back(); }
                else if (suffix == 'g') { multiplier = 1024ULL * 1024ULL * 1024ULL; num_part.pop_back(); }
            }

            try {
                size_t idx = 0;
                uint64_t val = std::stoull(num_part, &idx, 0); // Base 0 supports decimal, octal (0...), hex (0x...)
                return val * multiplier;
            } catch (...) {
                return 0;
            }
        }

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
                } else if (arg == "-b" || arg == "--print-bytes") {
                    opts.print_bytes = true;
                } else if (arg == "-l" || arg == "--verbose") {
                    opts.verbose_all = true;
                } else if (arg == "-s" || arg == "-q" || arg == "--silent" || arg == "--quiet") {
                    opts.silent = true;
                } else if (arg == "-n" || arg == "--bytes") {
                    if (i + 1 < argc) {
                        opts.limit_bytes = parse_size(argv[++i]);
                    } else {
                        std::cerr << "cmp: option requires an argument -- 'n'\n";
                        return false;
                    }
                } else if (arg.rfind("-n=", 0) == 0 || arg.rfind("--bytes=", 0) == 0) {
                    opts.limit_bytes = parse_size(arg.substr(arg.find('=') + 1));
                } else if (arg == "-i" || arg == "--ignore-initial") {
                    if (i + 1 < argc) {
                        parse_skip(argv[++i], opts);
                    } else {
                        std::cerr << "cmp: option requires an argument -- 'i'\n";
                        return false;
                    }
                } else if (arg.rfind("-i=", 0) == 0 || arg.rfind("--ignore-initial=", 0) == 0) {
                    parse_skip(arg.substr(arg.find('=') + 1), opts);
                } else if (arg.rfind("-", 0) == 0 && arg.length() > 1 && arg[1] != '-') {
                    // Grouped flags, e.g. -bls
                    for (size_t j = 1; j < arg.length(); ++j) {
                        switch (arg[j]) {
                            case 'b': opts.print_bytes = true; break;
                            case 'l': opts.verbose_all = true; break;
                            case 's':
                            case 'q': opts.silent = true; break;
                            default:
                                std::cerr << "cmp: invalid option -- '" << arg[j] << "'\n";
                                return false;
                        }
                    }
                } else {
                    positional.push_back(arg);
                }
            }

            if (positional.empty()) {
                std::cerr << "cmp: missing operand\nTry 'cmp --help' for more information.\n";
                return false;
            }

            opts.file1 = positional[0];
            if (positional.size() > 1) {
                opts.file2 = positional[1];
            } else {
                std::cerr << "cmp: missing operand after '" << opts.file1 << "'\n";
                return false;
            }

            // Positional skips support: cmp FILE1 FILE2 [SKIP1 [SKIP2]]
            if (positional.size() > 2) {
                opts.skip1 = parse_size(positional[2]);
            }
            if (positional.size() > 3) {
                opts.skip2 = parse_size(positional[3]);
            }

            return true;
        }

    private:
        static void parse_skip(const std::string& val, Options& opts) {
            auto colon_pos = val.find(':');
            if (colon_pos != std::string::npos) {
                opts.skip1 = parse_size(val.substr(0, colon_pos));
                opts.skip2 = parse_size(val.substr(colon_pos + 1));
            } else {
                opts.skip1 = parse_size(val);
                opts.skip2 = opts.skip1;
            }
        }
    };

    // =========================================================================
    // Help & Version Information
    // =========================================================================
    class HelpSystem {
    public:
        static void print_version() {
            std::cout << "cmp version 2.6.0\n"
                      << "Copyright (C) 2026, Roberto J Dohnert\n";
        }

        static void print_help() {
            std::cout << R"(cmp(1)                  CrossShell for UNIX Reference Manual                  cmp(1)

    NAME
        cmp - compare two byte streams

    SYNOPSIS
        cmp [OPTIONS] FILE1 [FILE2 [SKIP1 [SKIP2]]]

    DESCRIPTION
        Compares byte streams byte by byte, optionally skipping initial bytes and
        limiting the comparison length. Either file may be '-' for standard input.

    OPTIONS
        -b, --print-bytes
            Print differing byte values.

        -i, --ignore-initial VALUE
            Skip bytes; specify either SKIP or S1:S2.

        -l, --verbose
            Print byte numbers and differing byte values.

        -n, --bytes LIMIT
            Compare at most LIMIT bytes.

        -s, -q, --silent, --quiet
            Suppress normal comparison output.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version and license information.

    EXAMPLES
        cmp file1.bin file2.bin
            Compare two binary files byte by byte.

        cmp -l file1.bin file2.bin
            Output offsets and values of differing bytes.

        cmp -i 1k:2k -n 512 file1.bin file2.bin
            Skip initial bytes and compare first 512 bytes.

    CrossShell for UNIX                                                    cmp(1)
)";
        }
    };

    // =========================================================================
    // Main Application Orchestrator
    // =========================================================================
    class CmpApplication {
    public:
        static int execute(int argc, char* argv[]) {
            // Speed up C++ stream I/O
            std::ios_base::sync_with_stdio(false);
            std::cin.tie(nullptr);

            Options opts;
            if (!ArgParser::parse(argc, argv, opts)) {
                return static_cast<int>(ExitCode::Error);
            }

            if (opts.show_help) {
                HelpSystem::print_help();
                return static_cast<int>(ExitCode::Identical);
            }

            if (opts.show_version) {
                HelpSystem::print_version();
                return static_cast<int>(ExitCode::Identical);
            }

            FileComparator comparator(opts);
            return static_cast<int>(comparator.run());
        }
    };
}

int main(int argc, char* argv[]) {
    return CmpUtil::CmpApplication::execute(argc, argv);
}