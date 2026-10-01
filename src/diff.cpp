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
#include <cctype>
#include <map>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace DiffUtil {

    // Standard UNIX diff exit codes
    enum class ExitCode : int {
        Identical = 0,
        Differ    = 1,
        Error     = 2
    };

    enum class DiffFormat {
        Normal,
        Unified,
        Brief
    };

    struct Options {
        std::string file1;
        std::string file2;
        DiffFormat format = DiffFormat::Normal;
        int context_lines = 3;             // Default unified context lines
        bool ignore_case = false;          // -i
        bool ignore_all_space = false;     // -w
        bool ignore_space_change = false;  // -b
        bool ignore_blank_lines = false;   // -B
        bool report_identical = false;     // -s
        bool strip_trailing_cr = true;     // Clean Windows CRLF
        bool show_help = false;
        bool show_version = false;
    };

    enum class EditType {
        Keep,
        Insert,
        Delete
    };

    struct EditItem {
        EditType type;
        size_t index1; // 0-based index in file1 (if Keep/Delete)
        size_t index2; // 0-based index in file2 (if Keep/Insert)
    };

    // =========================================================================
    // Text Normalizer (Handles -i, -w, -b, -B)
    // =========================================================================
    class TextNormalizer {
    public:
        static std::string normalize(std::string_view line, const Options& opts) {
            std::string result;
            result.reserve(line.size());

            if (opts.ignore_all_space) {
                for (char ch : line) {
                    if (!std::isspace(static_cast<unsigned char>(ch))) {
                        result.push_back(opts.ignore_case ? static_cast<char>(std::tolower(static_cast<unsigned char>(ch))) : ch);
                    }
                }
                return result;
            }

            if (opts.ignore_space_change) {
                bool in_space = false;
                // Trim leading whitespace
                size_t start = 0;
                while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) {
                    ++start;
                }
                // Trim trailing whitespace
                size_t end = line.size();
                while (end > start && std::isspace(static_cast<unsigned char>(line[end - 1]))) {
                    --end;
                }

                for (size_t i = start; i < end; ++i) {
                    char ch = line[i];
                    if (std::isspace(static_cast<unsigned char>(ch))) {
                        if (!in_space) {
                            result.push_back(' ');
                            in_space = true;
                        }
                    } else {
                        result.push_back(opts.ignore_case ? static_cast<char>(std::tolower(static_cast<unsigned char>(ch))) : ch);
                        in_space = false;
                    }
                }
                return result;
            }

            if (opts.ignore_case) {
                for (char ch : line) {
                    result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
                }
                return result;
            }

            return std::string(line);
        }

        static bool is_blank(std::string_view line) {
            for (char ch : line) {
                if (!std::isspace(static_cast<unsigned char>(ch))) return false;
            }
            return true;
        }
    };

    // =========================================================================
    // Line Stream Reader (Files & Standard Input Piping)
    // =========================================================================
    class LineReader {
    public:
        static bool read_all_lines(const std::string& path, std::vector<std::string>& raw_lines,
                                  std::vector<std::string>& norm_lines, const Options& opts) {
            std::istream* stream_ptr = nullptr;
            std::unique_ptr<std::ifstream> file_stream;

            if (path == "-") {
#ifdef _WIN32
                if (_setmode(_fileno(stdin), _O_BINARY) == -1) return false;
#endif
                stream_ptr = &std::cin;
            } else {
                file_stream = std::make_unique<std::ifstream>(path, std::ios::binary);
                if (!file_stream->is_open()) {
                    std::cerr << "diff: " << path << ": No such file or directory\n";
                    return false;
                }
                stream_ptr = file_stream.get();
            }

            std::string line;
            while (std::getline(*stream_ptr, line)) {
                if (opts.strip_trailing_cr && !line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                if (opts.ignore_blank_lines && TextNormalizer::is_blank(line)) {
                    continue;
                }

                norm_lines.push_back(TextNormalizer::normalize(line, opts));
                raw_lines.push_back(std::move(line));
            }

            return true;
        }
    };

    // =========================================================================
    // Myers Diff Algorithm Engine
    // =========================================================================
    class MyersDiff {
    public:
        static std::vector<EditItem> compute(const std::vector<std::string>& a, const std::vector<std::string>& b) {
            const int N = static_cast<int>(a.size());
            const int M = static_cast<int>(b.size());
            const int MAX = N + M;

            if (MAX == 0) return {};

            std::vector<std::vector<int>> trace;
            std::vector<int> v(2 * MAX + 1, 0);

            for (int d = 0; d <= MAX; ++d) {
                trace.push_back(v);
                for (int k = -d; k <= d; k += 2) {
                    int k_idx = k + MAX;
                    int x = 0;

                    if (k == -d || (k != d && v[k_idx - 1] < v[k_idx + 1])) {
                        x = v[k_idx + 1]; // Move down (Insert from b)
                    } else {
                        x = v[k_idx - 1] + 1; // Move right (Delete from a)
                    }

                    int y = x - k;
                    while (x < N && y < M && a[x] == b[y]) {
                        x++;
                        y++;
                    }
                    v[k_idx] = x;

                    if (x >= N && y >= M) {
                        return backtrack(trace, a, b, MAX);
                    }
                }
            }

            return {};
        }

    private:
        static std::vector<EditItem> backtrack(const std::vector<std::vector<int>>& trace,
                                               const std::vector<std::string>& a,
                                               const std::vector<std::string>& b,
                                               int MAX) {
            int x = static_cast<int>(a.size());
            int y = static_cast<int>(b.size());
            std::vector<EditItem> edits;

            for (int d = static_cast<int>(trace.size()) - 1; d >= 0; --d) {
                const auto& v = trace[d];
                int k = x - y;
                int k_idx = k + MAX;

                int prev_k = 0;
                if (k == -d || (k != d && v[k_idx - 1] < v[k_idx + 1])) {
                    prev_k = k + 1;
                } else {
                    prev_k = k - 1;
                }

                int prev_x = v[prev_k + MAX];
                int prev_y = prev_x - prev_k;

                while (x > prev_x && y > prev_y) {
                    edits.push_back({EditType::Keep, static_cast<size_t>(x - 1), static_cast<size_t>(y - 1)});
                    x--;
                    y--;
                }

                if (d > 0) {
                    if (x == prev_x) {
                        edits.push_back({EditType::Insert, 0, static_cast<size_t>(y - 1)});
                    } else {
                        edits.push_back({EditType::Delete, static_cast<size_t>(x - 1), 0});
                    }
                }

                x = prev_x;
                y = prev_y;
            }

            std::reverse(edits.begin(), edits.end());
            return edits;
        }
    };

    // =========================================================================
    // Formatters (Normal, Unified, Brief)
    // =========================================================================
    class IOutputFormatter {
    public:
        virtual ~IOutputFormatter() = default;
        virtual void format(const std::vector<EditItem>& edits,
                            const std::vector<std::string>& lines1,
                            const std::vector<std::string>& lines2,
                            const Options& opts) = 0;
    };

    class BriefFormatter : public IOutputFormatter {
    public:
        void format(const std::vector<EditItem>&,
                    const std::vector<std::string>&,
                    const std::vector<std::string>&,
                    const Options& opts) override {
            std::cout << "Files " << opts.file1 << " and " << opts.file2 << " differ\n";
        }
    };

    class NormalFormatter : public IOutputFormatter {
    public:
        void format(const std::vector<EditItem>& edits,
                    const std::vector<std::string>& lines1,
                    const std::vector<std::string>& lines2,
                    const Options&) override {
            size_t i = 0;
            while (i < edits.size()) {
                if (edits[i].type == EditType::Keep) {
                    ++i;
                    continue;
                }

                size_t start_del = 0, end_del = 0;
                size_t start_ins = 0, end_ins = 0;
                bool has_del = false, has_ins = false;

                std::vector<size_t> del_indices;
                std::vector<size_t> ins_indices;

                while (i < edits.size() && edits[i].type != EditType::Keep) {
                    if (edits[i].type == EditType::Delete) {
                        if (!has_del) {
                            start_del = edits[i].index1 + 1;
                            has_del = true;
                        }
                        end_del = edits[i].index1 + 1;
                        del_indices.push_back(edits[i].index1);
                    } else if (edits[i].type == EditType::Insert) {
                        if (!has_ins) {
                            start_ins = edits[i].index2 + 1;
                            has_ins = true;
                        }
                        end_ins = edits[i].index2 + 1;
                        ins_indices.push_back(edits[i].index2);
                    }
                    ++i;
                }

                // Range output header
                if (has_del && has_ins) {
                    print_range(start_del, end_del);
                    std::cout << "c";
                    print_range(start_ins, end_ins);
                } else if (has_del) {
                    print_range(start_del, end_del);
                    std::cout << "d";
                    size_t anchor = ins_indices.empty() ? (i < edits.size() ? edits[i].index2 : lines2.size()) : start_ins;
                    std::cout << anchor;
                } else {
                    size_t anchor = del_indices.empty() ? (i < edits.size() ? edits[i].index1 : lines1.size()) : start_del;
                    std::cout << anchor << "a";
                    print_range(start_ins, end_ins);
                }
                std::cout << "\n";

                for (size_t idx : del_indices) {
                    std::cout << "< " << lines1[idx] << "\n";
                }
                if (has_del && has_ins) {
                    std::cout << "---\n";
                }
                for (size_t idx : ins_indices) {
                    std::cout << "> " << lines2[idx] << "\n";
                }
            }
        }

    private:
        static void print_range(size_t start, size_t end) {
            if (start == end) {
                std::cout << start;
            } else {
                std::cout << start << "," << end;
            }
        }
    };

    class UnifiedFormatter : public IOutputFormatter {
    public:
        void format(const std::vector<EditItem>& edits,
                    const std::vector<std::string>& lines1,
                    const std::vector<std::string>& lines2,
                    const Options& opts) override {
            std::cout << "--- " << opts.file1 << "\n";
            std::cout << "+++ " << opts.file2 << "\n";

            int ctx = opts.context_lines;
            size_t n = edits.size();
            size_t i = 0;

            while (i < n) {
                // Skip matching prefix
                while (i < n && edits[i].type == EditType::Keep) {
                    ++i;
                }
                if (i >= n) break;

                // Determine hunk bounds
                size_t hunk_start = (i > static_cast<size_t>(ctx)) ? (i - ctx) : 0;
                size_t hunk_end = i;

                while (hunk_end < n) {
                    if (edits[hunk_end].type != EditType::Keep) {
                        hunk_end++;
                    } else {
                        // Look ahead to see if next change is within 2 * ctx
                        size_t lookahead = hunk_end;
                        while (lookahead < n && edits[lookahead].type == EditType::Keep) {
                            lookahead++;
                        }
                        if (lookahead < n && (lookahead - hunk_end) <= static_cast<size_t>(2 * ctx)) {
                            hunk_end = lookahead;
                        } else {
                            hunk_end = std::min(n, hunk_end + ctx);
                            break;
                        }
                    }
                }

                // Compute hunk header coordinates
                size_t a_start = 0, a_count = 0;
                size_t b_start = 0, b_count = 0;
                bool found_a = false, found_b = false;

                for (size_t k = hunk_start; k < hunk_end; ++k) {
                    if (edits[k].type == EditType::Keep) {
                        if (!found_a) { a_start = edits[k].index1 + 1; found_a = true; }
                        if (!found_b) { b_start = edits[k].index2 + 1; found_b = true; }
                        a_count++;
                        b_count++;
                    } else if (edits[k].type == EditType::Delete) {
                        if (!found_a) { a_start = edits[k].index1 + 1; found_a = true; }
                        a_count++;
                    } else if (edits[k].type == EditType::Insert) {
                        if (!found_b) { b_start = edits[k].index2 + 1; found_b = true; }
                        b_count++;
                    }
                }

                if (!found_a) a_start = (hunk_start < n ? edits[hunk_start].index1 + 1 : lines1.size());
                if (!found_b) b_start = (hunk_start < n ? edits[hunk_start].index2 + 1 : lines2.size());

                std::cout << "@@ -" << a_start;
                if (a_count != 1) std::cout << "," << a_count;
                std::cout << " +" << b_start;
                if (b_count != 1) std::cout << "," << b_count;
                std::cout << " @@\n";

                // Print hunk contents
                for (size_t k = hunk_start; k < hunk_end; ++k) {
                    if (edits[k].type == EditType::Keep) {
                        std::cout << " " << lines1[edits[k].index1] << "\n";
                    } else if (edits[k].type == EditType::Delete) {
                        std::cout << "-" << lines1[edits[k].index1] << "\n";
                    } else if (edits[k].type == EditType::Insert) {
                        std::cout << "+" << lines2[edits[k].index2] << "\n";
                    }
                }

                i = hunk_end;
            }
        }
    };

    // =========================================================================
    // Command Line Parser
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
                } else if (arg == "-q" || arg == "--brief") {
                    opts.format = DiffFormat::Brief;
                } else if (arg == "-u") {
                    opts.format = DiffFormat::Unified;
                } else if (arg.rfind("-u=", 0) == 0 || arg.rfind("--unified=", 0) == 0) {
                    opts.format = DiffFormat::Unified;
                    opts.context_lines = std::stoi(arg.substr(arg.find('=') + 1));
                } else if (arg == "-U" || arg == "--unified") {
                    opts.format = DiffFormat::Unified;
                    if (i + 1 < argc && std::isdigit(argv[i + 1][0])) {
                        opts.context_lines = std::stoi(argv[++i]);
                    }
                } else if (arg.rfind("-U", 0) == 0 && arg.length() > 2) {
                    opts.format = DiffFormat::Unified;
                    opts.context_lines = std::stoi(arg.substr(2));
                } else if (arg == "-i" || arg == "--ignore-case") {
                    opts.ignore_case = true;
                } else if (arg == "-w" || arg == "--ignore-all-space") {
                    opts.ignore_all_space = true;
                } else if (arg == "-b" || arg == "--ignore-space-change") {
                    opts.ignore_space_change = true;
                } else if (arg == "-B" || arg == "--ignore-blank-lines") {
                    opts.ignore_blank_lines = true;
                } else if (arg == "-s" || arg == "--report-identical-files") {
                    opts.report_identical = true;
                } else if (arg == "--strip-trailing-cr") {
                    opts.strip_trailing_cr = true;
                } else if (arg.length() > 1 && arg[0] == '-' && arg != "-") {
                    // Support clustered single-char flags like -qi, -ub
                    for (size_t j = 1; j < arg.length(); ++j) {
                        switch (arg[j]) {
                            case 'q': opts.format = DiffFormat::Brief; break;
                            case 'u': opts.format = DiffFormat::Unified; break;
                            case 'i': opts.ignore_case = true; break;
                            case 'w': opts.ignore_all_space = true; break;
                            case 'b': opts.ignore_space_change = true; break;
                            case 'B': opts.ignore_blank_lines = true; break;
                            case 's': opts.report_identical = true; break;
                            default:
                                std::cerr << "diff: invalid option -- '" << arg[j] << "'\n";
                                return false;
                        }
                    }
                } else {
                    positional.push_back(arg);
                }
            }

            if (opts.show_help || opts.show_version) return true;

            if (positional.size() < 2) {
                std::cerr << "diff: missing operand after '" << (positional.empty() ? "" : positional[0]) << "'\n"
                          << "Try 'diff --help' for more information.\n";
                return false;
            }

            opts.file1 = positional[0];
            opts.file2 = positional[1];
            return true;
        }
    };

    // =========================================================================
    // Help & Information System
    // =========================================================================
    class HelpSystem {
    public:
        static void print_version() {
            std::cout << "diff version 3.1.0\n"
                      << "Copyright (C) 2026, Roberto J Dohnert.\n";
        }

        static void print_help() {
            std::cout << R"(diff(1)                 CrossShell for UNIX Reference Manual                  diff(1)

    NAME
        diff - compare files line by line

    SYNOPSIS
        diff [OPTIONS] FILE1 FILE2

    DESCRIPTION
        Compare files line by line and report differences. When FILE1 or FILE2
        is '-', read standard input.

    OPTIONS
        -u, -U NUM, --unified[=NUM]
            Output NUM (default: 3) lines of unified context.

        -q, --brief
            Report only when files differ.

        -i, --ignore-case
            Ignore case differences in file contents.

        -w, --ignore-all-space
            Ignore all white space.

        -b, --ignore-space-change
            Ignore changes in the amount of white space.

        -B, --ignore-blank-lines
            Ignore changes where lines are all blank.

        -s, --report-identical-files
            Report when two files are identical.

        --strip-trailing-cr
            Strip trailing carriage return on input.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version and license information.

    EXAMPLES
        diff file1.txt file2.txt
            Compare files in standard normal diff format.

        diff -u -U 5 file1.txt file2.txt
            Produce unified diff with 5 context lines.

        diff -iwb file1.txt file2.txt
            Ignore case and whitespace changes while comparing.

    CrossShell for UNIX                                                    diff(1)
)";
        }
    };

    // =========================================================================
    // Main Application Coordinator
    // =========================================================================
    class DiffApp {
    public:
        static int run(int argc, char* argv[]) {
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

            std::vector<std::string> raw1, raw2;
            std::vector<std::string> norm1, norm2;

            if (!LineReader::read_all_lines(opts.file1, raw1, norm1, opts)) return static_cast<int>(ExitCode::Error);
            if (!LineReader::read_all_lines(opts.file2, raw2, norm2, opts)) return static_cast<int>(ExitCode::Error);

            auto edits = MyersDiff::compute(norm1, norm2);

            bool differs = false;
            for (const auto& item : edits) {
                if (item.type != EditType::Keep) {
                    differs = true;
                    break;
                }
            }

            if (!differs) {
                if (opts.report_identical) {
                    std::cout << "Files " << opts.file1 << " and " << opts.file2 << " are identical\n";
                }
                return static_cast<int>(ExitCode::Identical);
            }

            std::unique_ptr<IOutputFormatter> formatter;
            if (opts.format == DiffFormat::Brief) {
                formatter = std::make_unique<BriefFormatter>();
            } else if (opts.format == DiffFormat::Unified) {
                formatter = std::make_unique<UnifiedFormatter>();
            } else {
                formatter = std::make_unique<NormalFormatter>();
            }

            formatter->format(edits, raw1, raw2, opts);
            return static_cast<int>(ExitCode::Differ);
        }
    };
}

int main(int argc, char* argv[]) {
    return DiffUtil::DiffApp::run(argc, argv);
}