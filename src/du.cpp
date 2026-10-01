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
#include <filesystem>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace fs = std::filesystem;

namespace DuUtil {

    // Program Exit Codes
    enum class ExitCode : int {
        Success = 0,
        Failure = 1
    };

    enum class SizeFormatMode {
        Kilobytes,     // Default: 1024-byte blocks
        Megabytes,     // -m: 1048576-byte blocks
        Bytes,         // -b: raw bytes
        HumanReadable  // -H: 1K, 234M, 2G
    };

    // Configuration Settings
    struct Options {
        std::vector<std::string> paths;
        bool all_files = false;               // -a, --all
        bool summarize_only = false;          // -s, --summarize
        bool total = false;                   // -c, --total
        bool apparent_size = false;           // -b, --apparent-size
        bool null_terminated = false;         // -0, --null
        bool dereference = false;             // -L, --dereference
        int max_depth = -1;                   // -d N, --max-depth=N (-1 = unlimited)
        SizeFormatMode format_mode = SizeFormatMode::Kilobytes;
        uint64_t block_size = 1024;
        uint64_t cluster_size = 4096;         // Standard Windows cluster size (4 KB)
        std::vector<std::string> exclude_patterns;
        std::string files0_from;
        bool show_help = false;
        bool show_version = false;
        int output_format = 0;
        std::string pipe_command;
    };

    // =========================================================================
    // Size Formatter Utility (Strategy Pattern)
    // =========================================================================
    class SizeFormatter {
    public:
        static std::string format(uint64_t raw_bytes, const Options& opts) {
            if (opts.format_mode == SizeFormatMode::Bytes) {
                return std::to_string(raw_bytes);
            }

            if (opts.format_mode == SizeFormatMode::HumanReadable) {
                return to_human_readable(raw_bytes);
            }

            // Block size divisions (-k, -m, default 1K)
            uint64_t blocks = (raw_bytes + opts.block_size - 1) / opts.block_size;
            return std::to_string(blocks);
        }

    private:
        static std::string to_human_readable(uint64_t bytes) {
            constexpr double unit = 1024.0;
            if (bytes < 1024) {
                return std::to_string(bytes) + "B";
            }

            const char* suffixes[] = { "K", "M", "G", "T", "P", "E" };
            double count = static_cast<double>(bytes);
            int exp = 0;

            while (count >= unit && exp < 6) {
                count /= unit;
                exp++;
            }

            std::ostringstream oss;
            if (count >= 10.0 || exp == 0) {
                oss << static_cast<uint64_t>(std::round(count)) << suffixes[exp - 1];
            } else {
                oss << std::fixed << std::setprecision(1) << count << suffixes[exp - 1];
            }
            return oss.str();
        }
    };

    // =========================================================================
    // Wildcard Filter (Handles --exclude=PATTERN)
    // =========================================================================
    class PathFilter {
    public:
        static bool is_excluded(const std::string& filename, const std::vector<std::string>& patterns) {
            for (const auto& pattern : patterns) {
                if (wildcard_match(pattern, filename)) {
                    return true;
                }
            }
            return false;
        }

    private:
        static bool wildcard_match(std::string_view pattern, std::string_view str) {
            size_t p_idx = 0, s_idx = 0;
            size_t star_idx = std::string_view::npos, match_idx = 0;

            while (s_idx < str.length()) {
                if (p_idx < pattern.length() && (pattern[p_idx] == '?' || pattern[p_idx] == str[s_idx])) {
                    p_idx++;
                    s_idx++;
                } else if (p_idx < pattern.length() && pattern[p_idx] == '*') {
                    star_idx = p_idx;
                    match_idx = s_idx;
                    p_idx++;
                } else if (star_idx != std::string_view::npos) {
                    p_idx = star_idx + 1;
                    match_idx++;
                    s_idx = match_idx;
                } else {
                    return false;
                }
            }

            while (p_idx < pattern.length() && pattern[p_idx] == '*') {
                p_idx++;
            }

            return p_idx == pattern.length();
        }
    };

    // =========================================================================
    // Pipe / Stream Path Ingestion
    // =========================================================================
    class InputStreamReader {
    public:
        static std::vector<std::string> read_paths_from_stream(std::istream& in, char delimiter) {
            std::vector<std::string> paths;
            std::string path;
            while (std::getline(in, path, delimiter)) {
                if (delimiter == '\n' && !path.empty() && path.back() == '\r') {
                    path.pop_back();
                }
                if (!path.empty()) {
                    paths.push_back(path);
                }
            }
            return paths;
        }
    };

    // =========================================================================
    // Recursive Disk Usage Scanner
    // =========================================================================
    class DiskUsageScanner {
    private:
        Options opts_;
        uint64_t grand_total_bytes_ = 0;

    public:
        explicit DiskUsageScanner(Options opts) : opts_(std::move(opts)) {
            if (opts_.summarize_only) {
                opts_.max_depth = 0;
            }
        }

        bool scan_all(const std::vector<std::string>& targets) {
            bool all_success = true;

            for (const auto& target_str : targets) {
                fs::path target(target_str);
                std::error_code ec;

                if (!fs::exists(target, ec)) {
                    std::cerr << "du: cannot access '" << target_str << "': No such file or directory\n";
                    all_success = false;
                    continue;
                }

                uint64_t path_bytes = 0;
                if (fs::is_directory(target, ec)) {
                    path_bytes = scan_directory(target, 0);
                } else {
                    path_bytes = calculate_item_size(target);
                    print_entry(path_bytes, target.string());
                }
                grand_total_bytes_ += path_bytes;
            }

            if (opts_.total) {
                print_entry(grand_total_bytes_, "total");
            }

            return all_success;
        }

    private:
        uint64_t calculate_item_size(const fs::path& p) {
            std::error_code ec;
            if (fs::is_symlink(p, ec) && !opts_.dereference) {
                return 0;
            }

            uint64_t size = fs::file_size(p, ec);
            if (ec) return 0;

            if (opts_.apparent_size) {
                return size;
            }

            // Calculate disk allocation size rounded up to cluster size
            if (size == 0) return 0;
            return ((size + opts_.cluster_size - 1) / opts_.cluster_size) * opts_.cluster_size;
        }

        uint64_t scan_directory(const fs::path& dir, int current_depth) {
            std::error_code ec;
            uint64_t dir_total = 0;

            if (PathFilter::is_excluded(dir.filename().string(), opts_.exclude_patterns)) {
                return 0;
            }

            auto iter_opts = fs::directory_options::skip_permission_denied;
            if (opts_.dereference) {
                iter_opts |= fs::directory_options::follow_directory_symlink;
            }

            for (const auto& entry : fs::directory_iterator(dir, iter_opts, ec)) {
                const auto& item_path = entry.path();
                std::string item_name = item_path.filename().string();

                if (PathFilter::is_excluded(item_name, opts_.exclude_patterns)) {
                    continue;
                }

                if (entry.is_directory(ec)) {
                    uint64_t sub_size = scan_directory(item_path, current_depth + 1);
                    dir_total += sub_size;
                } else {
                    uint64_t file_size = calculate_item_size(item_path);
                    dir_total += file_size;

                    if (opts_.all_files) {
                        if (opts_.max_depth == -1 || current_depth + 1 <= opts_.max_depth) {
                            print_entry(file_size, item_path.string());
                        }
                    }
                }
            }

            if (opts_.max_depth == -1 || current_depth <= opts_.max_depth) {
                print_entry(dir_total, dir.string());
            }

            return dir_total;
        }

        void print_entry(uint64_t bytes, const std::string& name) const {
            std::cout << SizeFormatter::format(bytes, opts_) << "\t" << name
                      << (opts_.null_terminated ? '\0' : '\n');
        }
    };

    // =========================================================================
    // Command Line Parser
    // =========================================================================
    class ArgParser {
    public:
        static bool parse(int argc, char* argv[], Options& opts) {
            for (int i = 1; i < argc; ++i) {
                std::string arg = argv[i];

                if (arg == "--help" || arg == "-h" || arg == "-?") {
                    opts.show_help = true;
                    return true;
                } else if (arg == "--version" || arg == "-v") {
                    opts.show_version = true;
                    return true;
                } else if (arg == "-H" || arg == "--human-readable") {
                    opts.format_mode = SizeFormatMode::HumanReadable;
                } else if (arg == "-a" || arg == "--all") {
                    opts.all_files = true;
                } else if (arg == "-s" || arg == "--summarize") {
                    opts.summarize_only = true;
                } else if (arg == "-c" || arg == "--total") {
                    opts.total = true;
                } else if (arg == "-k") {
                    opts.format_mode = SizeFormatMode::Kilobytes;
                    opts.block_size = 1024;
                } else if (arg == "-m") {
                    opts.format_mode = SizeFormatMode::Megabytes;
                    opts.block_size = 1024 * 1024;
                } else if (arg == "-b" || arg == "--bytes") {
                    opts.format_mode = SizeFormatMode::Bytes;
                    opts.apparent_size = true;
                } else if (arg == "--apparent-size") {
                    opts.apparent_size = true;
                } else if (arg == "-0" || arg == "--null") {
                    opts.null_terminated = true;
                } else if (arg == "-L" || arg == "--dereference") {
                    opts.dereference = true;
                } else if (arg == "-d" || arg == "--max-depth") {
                    if (i + 1 < argc) {
                        opts.max_depth = std::stoi(argv[++i]);
                    } else {
                        std::cerr << "du: option requires an argument -- 'd'\n";
                        return false;
                    }
                } else if (arg.rfind("-d=", 0) == 0 || arg.rfind("--max-depth=", 0) == 0) {
                    opts.max_depth = std::stoi(arg.substr(arg.find('=') + 1));
                } else if (arg.rfind("--exclude=", 0) == 0) {
                    opts.exclude_patterns.push_back(arg.substr(10));
                } else if (arg == "--exclude") {
                    if (i + 1 < argc) {
                        opts.exclude_patterns.push_back(argv[++i]);
                    } else {
                        std::cerr << "du: option '--exclude' requires an argument\n";
                        return false;
                    }
                } else if (arg.rfind("--files0-from=", 0) == 0) {
                    opts.files0_from = arg.substr(14);
                } else if (arg == "--json") {
                    opts.output_format = 1;
                } else if (arg == "--csv") {
                    opts.output_format = 2;
                } else if (arg == "--table") {
                    opts.output_format = 3;
                } else if (arg == "--pipe" && i + 1 < argc) {
                    opts.pipe_command = argv[++i];
                } else if (arg.length() > 1 && arg[0] == '-' && arg != "-") {
                    // Clustered short options (e.g. -scH, -ak)
                    for (size_t j = 1; j < arg.length(); ++j) {
                        switch (arg[j]) {
                            case 'H': opts.format_mode = SizeFormatMode::HumanReadable; break;
                            case 'h': opts.show_help = true; return true;
                            case 'a': opts.all_files = true; break;
                            case 's': opts.summarize_only = true; break;
                            case 'c': opts.total = true; break;
                            case 'k': opts.format_mode = SizeFormatMode::Kilobytes; opts.block_size = 1024; break;
                            case 'm': opts.format_mode = SizeFormatMode::Megabytes; opts.block_size = 1024 * 1024; break;
                            case 'b': opts.format_mode = SizeFormatMode::Bytes; opts.apparent_size = true; break;
                            case '0': opts.null_terminated = true; break;
                            case 'L': opts.dereference = true; break;
                            default:
                                std::cerr << "du: invalid option -- '" << arg[j] << "'\n"
                                          << "Try 'du --help' for more information.\n";
                                return false;
                        }
                    }
                } else {
                    opts.paths.push_back(arg);
                }
            }
            return true;
        }
    };

    // =========================================================================
    // Help & Information Subsystem
    // =========================================================================
    class HelpSystem {
    public:
        static void print_version() {
            std::cout << "du version 5.1.0\n"
                      << "Copyright (C) 2026, Roberto J Dohnert.\n";
        }

        static void print_help() {
            std::cout << R"(du(1)                     CrossShell for UNIX Reference Manual                     du(1)

    NAME
        du - estimate file and directory space usage

    SYNOPSIS
        du [OPTIONS] [FILE]...

    DESCRIPTION
        Reports Windows file and directory sizes using block, byte, or
        human-readable units. With no FILE specified, the current directory
        is scanned recursively.

        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -a, --all
            Include individual files, not only directory totals.

        -s, --summarize
            Display only a total summary for each argument.

        -c, --total
            Produce a cumulative grand total.

        -b, --apparent-size
            Print apparent sizes rather than disk allocation usage.

        -0, --null
            End each output line with a NUL character instead of newline.

        -L, --dereference
            Follow all symbolic links and reparse points.

        -d, --max-depth=N
            Print directory total only if N or fewer levels below argument.

        -k, --kilobytes
            Use 1024-byte blocks (default).

        -m, --megabytes
            Use 1048576-byte (1 MB) blocks.

        -H, --human-readable
            Print sizes in human-readable format (e.g., 1K, 234M, 2G).

        --exclude=PATTERN
            Exclude files and directories matching PATTERN.

        --files0-from=FILE
            Summarize usage of NUL-terminated paths specified in FILE.

        --json
            Output usage entries as JSON.

        --csv
            Output usage entries as CSV.

        --table
            Output usage entries as a formatted table.

        --pipe COMMAND
            Send usage output through COMMAND pipeline.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Output version information and exit.

    EXAMPLES
        du -h C:\Projects
            Estimate disk usage in C:\Projects using human-readable units.

        du -a --max-depth=2 .
            List all files and directories up to depth 2 in current directory.

        du -s -c C:\Users\Public C:\Temp
            Summarize disk usage for specified directories with a total.

        du --exclude=*.obj --json build
            Estimate usage in build directory excluding *.obj files in JSON.

    CrossShell for UNIX                                                        du(1)
)";
        }
    };

    // =========================================================================
    // Main Application Coordinator
    // =========================================================================
    class DuApp {
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

            std::vector<std::string> final_paths;

            // Ingestion via --files0-from
            if (!opts.files0_from.empty()) {
                if (opts.files0_from == "-") {
#ifdef _WIN32
                    if (_setmode(_fileno(stdin), _O_BINARY) == -1) return static_cast<int>(ExitCode::Failure);
#endif
                    final_paths = InputStreamReader::read_paths_from_stream(std::cin, '\0');
                } else {
                    std::ifstream f(opts.files0_from, std::ios::binary);
                    if (!f.is_open()) {
                        std::cerr << "du: cannot open '" << opts.files0_from << "' for reading: No such file or directory\n";
                        return static_cast<int>(ExitCode::Failure);
                    }
                    final_paths = InputStreamReader::read_paths_from_stream(f, '\0');
                }
            } else {
                // Ingestion via positional arguments or stdin '-'
                if (opts.paths.empty()) {
                    final_paths.push_back(".");
                } else {
                    for (const auto& p : opts.paths) {
                        if (p == "-") {
#ifdef _WIN32
                            if (_setmode(_fileno(stdin), _O_BINARY) == -1) return static_cast<int>(ExitCode::Failure);
#endif
                            auto piped_paths = InputStreamReader::read_paths_from_stream(std::cin, '\n');
                            final_paths.insert(final_paths.end(), piped_paths.begin(), piped_paths.end());
                        } else {
                            final_paths.push_back(p);
                        }
                    }
                }
            }

            DiskUsageScanner scanner(opts);
            std::ostringstream captured;
            std::streambuf* oldOutput = nullptr;
            if (opts.output_format || !opts.pipe_command.empty()) oldOutput = std::cout.rdbuf(captured.rdbuf());
            bool success = scanner.scan_all(final_paths);
            if (oldOutput) {
                std::cout.rdbuf(oldOutput);
                std::string data = captured.str();
                std::string text = opts.output_format == 1 ? "{\"output\":\"" + data + "\"}\n" : opts.output_format == 2 ? "\"output\"\n\"" + data + "\"\n" : "OUTPUT\n------\n" + data;
                if (!opts.pipe_command.empty()) { FILE* pipe = _popen(opts.pipe_command.c_str(), "w"); if (pipe) { std::fwrite(text.data(), 1, text.size(), pipe); _pclose(pipe); } }
                else std::cout << text;
            }
            return success ? static_cast<int>(ExitCode::Success) : static_cast<int>(ExitCode::Failure);
        }
    };
}

int main(int argc, char* argv[]) {
    return DuUtil::DuApp::run(argc, argv);
}