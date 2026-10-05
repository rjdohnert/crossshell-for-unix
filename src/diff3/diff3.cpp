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

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace Diff3Util {

    // Program Exit Codes
    enum class ExitCode : int {
        SuccessNoConflicts = 0,
        ConflictsFound     = 1,
        Error              = 2
    };

    enum class OutputMode {
        Traditional,   // Standard ==== 3-way diff
        Merge,         // -m: Standard 3-way merge with conflict markers
        EdAll,         // -e: ed script incorporating all changes
        EdOverlap,     // -x: ed script with only overlapping changes
        EdNonOverlap,  // -3: ed script with only non-overlapping changes
        EdShowOverlap  // -E / -X: ed script with bracketed conflicts
    };

    struct Options {
        std::string file_mine;  // FILE1 (MYFILE / MINE)
        std::string file_old;   // FILE2 (OLDFILE / BASE / ANCESTOR)
        std::string file_yours; // FILE3 (YOURFILE / YOUNGER / THEIRS)

        std::vector<std::string> labels; // Custom labels for conflict markers
        OutputMode mode = OutputMode::Traditional;

        bool show_base_in_conflict = false; // -A / ||||||| markers
        bool initial_tab = false;          // -T / -i
        bool strip_trailing_cr = true;     // Clean Windows CRLF
        bool show_help = false;
        bool show_version = false;
    };

    enum class EditType { Keep, Insert, Delete };

    struct EditItem {
        EditType type;
        size_t index_base; // Index in base
        size_t index_mod;  // Index in modified file
    };

    // Represents an aligned chunk of modifications across the 3 files
    enum class ChunkType {
        Unchanged,
        MineOnly,
        YoursOnly,
        IdenticalChange,
        Conflict
    };

    struct Diff3Chunk {
        ChunkType type;
        // 0-based index ranges [start, end)
        size_t base_start = 0, base_end = 0;
        size_t mine_start = 0, mine_end = 0;
        size_t yours_start = 0, yours_end = 0;

        std::vector<std::string> mine_lines;
        std::vector<std::string> base_lines;
        std::vector<std::string> yours_lines;
    };

    // =========================================================================
    // Line Reader (Handles Files, Stdin Piping, and Windows Line Endings)
    // =========================================================================
    class LineReader {
    public:
        static bool read_file(const std::string& path, std::vector<std::string>& lines, bool strip_cr) {
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
                    std::cerr << "diff3: " << path << ": No such file or directory\n";
                    return false;
                }
                stream_ptr = file_stream.get();
            }

            std::string line;
            while (std::getline(*stream_ptr, line)) {
                if (strip_cr && !line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                lines.push_back(std::move(line));
            }
            return true;
        }
    };

    // =========================================================================
    // Myers Diff Algorithm Engine (Pairwise Diff)
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
                        x = v[k_idx + 1];
                    } else {
                        x = v[k_idx - 1] + 1;
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
                                               const std::vector<std::string>&,
                                               const std::vector<std::string>&,
                                               int MAX) {
            int x = static_cast<int>(trace[0].size() > 0 ? (trace.back()[MAX] >= 0 ? trace.back()[MAX] : 0) : 0);
            int y = 0;
            // Get actual dimensions
            int d_max = static_cast<int>(trace.size()) - 1;
            x = trace[d_max][MAX + (static_cast<int>(trace[0].size()) / 2)]; 
            
            // Fast reverse traversal
            std::vector<EditItem> edits;
            int cur_x = 0;
            int cur_y = 0;
            
            // To ensure robust extraction across arbitrary inputs, reconstruct from forward edits:
            return reconstruct_edits(trace, MAX);
        }

        static std::vector<EditItem> reconstruct_edits(const std::vector<std::vector<int>>& trace, int MAX) {
            std::vector<EditItem> edits;
            // Determine endpoints
            int d_end = static_cast<int>(trace.size()) - 1;
            int end_x = 0, end_y = 0;
            int best_k = 0;
            
            for (int k = -d_end; k <= d_end; k += 2) {
                int x = trace[d_end][k + MAX];
                int y = x - k;
                if (x >= end_x && y >= end_y) {
                    end_x = x;
                    end_y = y;
                    best_k = k;
                }
            }

            int x = end_x;
            int y = end_y;

            for (int d = d_end; d >= 0; --d) {
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
    // 3-Way Alignment Engine
    // =========================================================================
    class Diff3Engine {
    public:
        static std::vector<Diff3Chunk> align(const std::vector<std::string>& mine,
                                            const std::vector<std::string>& base,
                                            const std::vector<std::string>& yours) {
            auto edits_mine = MyersDiff::compute(base, mine);
            auto edits_yours = MyersDiff::compute(base, yours);

            // Group edits by base lines
            size_t base_size = base.size();
            std::vector<std::vector<std::string>> mine_ins(base_size + 1);
            std::vector<std::vector<std::string>> yours_ins(base_size + 1);
            std::vector<bool> mine_deleted(base_size, false);
            std::vector<bool> yours_deleted(base_size, false);

            size_t cur_base = 0;
            for (const auto& e : edits_mine) {
                if (e.type == EditType::Insert) {
                    mine_ins[cur_base].push_back(mine[e.index_mod]);
                } else if (e.type == EditType::Delete) {
                    mine_deleted[e.index_base] = true;
                    cur_base = e.index_base + 1;
                } else {
                    cur_base = e.index_base + 1;
                }
            }

            cur_base = 0;
            for (const auto& e : edits_yours) {
                if (e.type == EditType::Insert) {
                    yours_ins[cur_base].push_back(yours[e.index_mod]);
                } else if (e.type == EditType::Delete) {
                    yours_deleted[e.index_base] = true;
                    cur_base = e.index_base + 1;
                } else {
                    cur_base = e.index_base + 1;
                }
            }

            std::vector<Diff3Chunk> chunks;
            size_t b_idx = 0;
            size_t m_line_tracker = 0;
            size_t y_line_tracker = 0;

            while (b_idx <= base_size) {
                bool has_mine_change = !mine_ins[b_idx].empty() || (b_idx < base_size && mine_deleted[b_idx]);
                bool has_yours_change = !yours_ins[b_idx].empty() || (b_idx < base_size && yours_deleted[b_idx]);

                if (!has_mine_change && !has_yours_change) {
                    if (b_idx < base_size) {
                        Diff3Chunk chunk;
                        chunk.type = ChunkType::Unchanged;
                        chunk.base_start = b_idx;
                        chunk.base_end = b_idx + 1;
                        chunk.mine_start = m_line_tracker;
                        chunk.mine_end = m_line_tracker + 1;
                        chunk.yours_start = y_line_tracker;
                        chunk.yours_end = y_line_tracker + 1;

                        chunk.base_lines.push_back(base[b_idx]);
                        chunk.mine_lines.push_back(base[b_idx]);
                        chunk.yours_lines.push_back(base[b_idx]);

                        chunks.push_back(std::move(chunk));
                        m_line_tracker++;
                        y_line_tracker++;
                    }
                    b_idx++;
                } else {
                    // Accumulate contiguous changed block
                    size_t b_start = b_idx;
                    size_t m_start = m_line_tracker;
                    size_t y_start = y_line_tracker;

                    std::vector<std::string> b_block;
                    std::vector<std::string> m_block;
                    std::vector<std::string> y_block;

                    while (b_idx <= base_size && 
                          (!mine_ins[b_idx].empty() || !yours_ins[b_idx].empty() || 
                          (b_idx < base_size && (mine_deleted[b_idx] || yours_deleted[b_idx])))) {
                        
                        for (const auto& s : mine_ins[b_idx]) { m_block.push_back(s); m_line_tracker++; }
                        for (const auto& s : yours_ins[b_idx]) { y_block.push_back(s); y_line_tracker++; }

                        if (b_idx < base_size) {
                            b_block.push_back(base[b_idx]);
                            if (!mine_deleted[b_idx]) {
                                m_block.push_back(base[b_idx]);
                                m_line_tracker++;
                            }
                            if (!yours_deleted[b_idx]) {
                                y_block.push_back(base[b_idx]);
                                y_line_tracker++;
                            }
                        }
                        b_idx++;
                    }

                    Diff3Chunk chunk;
                    chunk.base_start = b_start;
                    chunk.base_end = b_idx;
                    chunk.mine_start = m_start;
                    chunk.mine_end = m_line_tracker;
                    chunk.yours_start = y_start;
                    chunk.yours_end = y_line_tracker;

                    chunk.base_lines = b_block;
                    chunk.mine_lines = m_block;
                    chunk.yours_lines = y_block;

                    bool mine_diff = (m_block != b_block);
                    bool yours_diff = (y_block != b_block);

                    if (mine_diff && yours_diff) {
                        if (m_block == y_block) {
                            chunk.type = ChunkType::IdenticalChange;
                        } else {
                            chunk.type = ChunkType::Conflict;
                        }
                    } else if (mine_diff) {
                        chunk.type = ChunkType::MineOnly;
                    } else if (yours_diff) {
                        chunk.type = ChunkType::YoursOnly;
                    } else {
                        chunk.type = ChunkType::Unchanged;
                    }

                    chunks.push_back(std::move(chunk));
                }
            }

            return chunks;
        }
    };

    // =========================================================================
    // Output Formatters
    // =========================================================================
    class IOutputFormatter {
    public:
        virtual ~IOutputFormatter() = default;
        virtual bool format(const std::vector<Diff3Chunk>& chunks,
                            const Options& opts) = 0;
    };

    // Standard traditional diff3 output format
    class TraditionalFormatter : public IOutputFormatter {
    public:
        bool format(const std::vector<Diff3Chunk>& chunks, const Options& opts) override {
            bool has_diff = false;

            for (const auto& c : chunks) {
                if (c.type == ChunkType::Unchanged) continue;
                has_diff = true;

                if (c.type == ChunkType::Conflict) {
                    std::cout << "====\n";
                } else if (c.type == ChunkType::MineOnly) {
                    std::cout << "====1\n";
                } else if (c.type == ChunkType::YoursOnly) {
                    std::cout << "====3\n";
                } else if (c.type == ChunkType::IdenticalChange) {
                    std::cout << "====2\n";
                }

                print_range(1, c.mine_start + 1, c.mine_end);
                print_range(2, c.base_start + 1, c.base_end);
                print_range(3, c.yours_start + 1, c.yours_end);

                std::string prefix = opts.initial_tab ? "\t\t" : "  ";

                if (c.type == ChunkType::Conflict || c.type == ChunkType::MineOnly) {
                    for (const auto& l : c.mine_lines) std::cout << "1:" << prefix << l << "\n";
                }
                if (c.type == ChunkType::Conflict || c.type == ChunkType::IdenticalChange) {
                    for (const auto& l : c.base_lines) std::cout << "2:" << prefix << l << "\n";
                }
                if (c.type == ChunkType::Conflict || c.type == ChunkType::YoursOnly) {
                    for (const auto& l : c.yours_lines) std::cout << "3:" << prefix << l << "\n";
                }
            }
            return has_diff;
        }

    private:
        static void print_range(int file_no, size_t start, size_t end) {
            std::cout << file_no << ":";
            if (start > end) {
                std::cout << (start - 1) << "a\n";
            } else if (start == end) {
                std::cout << start << "c\n";
            } else {
                std::cout << start << "," << end << "c\n";
            }
        }
    };

    // 3-Way Merge Formatter (-m, -A, -E)
    class MergeFormatter : public IOutputFormatter {
    public:
        bool format(const std::vector<Diff3Chunk>& chunks, const Options& opts) override {
            bool conflict_present = false;

            std::string label1 = opts.labels.size() > 0 ? opts.labels[0] : opts.file_mine;
            std::string label2 = opts.labels.size() > 1 ? opts.labels[1] : opts.file_old;
            std::string label3 = opts.labels.size() > 2 ? opts.labels[2] : opts.file_yours;

            for (const auto& c : chunks) {
                switch (c.type) {
                    case ChunkType::Unchanged:
                    case ChunkType::MineOnly:
                        for (const auto& line : c.mine_lines) std::cout << line << "\n";
                        break;
                    case ChunkType::YoursOnly:
                    case ChunkType::IdenticalChange:
                        for (const auto& line : c.yours_lines) std::cout << line << "\n";
                        break;
                    case ChunkType::Conflict:
                        conflict_present = true;
                        std::cout << "<<<<<<< " << label1 << "\n";
                        for (const auto& line : c.mine_lines) std::cout << line << "\n";
                        if (opts.show_base_in_conflict) {
                            std::cout << "||||||| " << label2 << "\n";
                            for (const auto& line : c.base_lines) std::cout << line << "\n";
                        }
                        std::cout << "=======\n";
                        for (const auto& line : c.yours_lines) std::cout << line << "\n";
                        std::cout << ">>>>>>> " << label3 << "\n";
                        break;
                }
            }
            return conflict_present;
        }
    };

    // Ed Script Formatter (-e, -E, -3, -x)
    class EdFormatter : public IOutputFormatter {
    public:
        bool format(const std::vector<Diff3Chunk>& chunks, const Options& opts) override {
            bool conflict_present = false;
            // Iterate in reverse for ed commands so line numbers remain valid
            for (auto it = chunks.rbegin(); it != chunks.rend(); ++it) {
                const auto& c = *it;
                bool output_chunk = false;
                bool is_overlap = (c.type == ChunkType::Conflict);

                if (is_overlap) conflict_present = true;

                if (opts.mode == OutputMode::EdAll) {
                    output_chunk = (c.type != ChunkType::Unchanged && c.type != ChunkType::MineOnly);
                } else if (opts.mode == OutputMode::EdOverlap) {
                    output_chunk = is_overlap;
                } else if (opts.mode == OutputMode::EdNonOverlap) {
                    output_chunk = (c.type == ChunkType::YoursOnly || c.type == ChunkType::IdenticalChange);
                } else if (opts.mode == OutputMode::EdShowOverlap) {
                    output_chunk = (c.type != ChunkType::Unchanged && c.type != ChunkType::MineOnly);
                }

                if (!output_chunk) continue;

                size_t start = c.mine_start + 1;
                size_t end = c.mine_end;

                if (start > end) {
                    std::cout << (start - 1) << "a\n";
                } else if (start == end) {
                    std::cout << start << "c\n";
                } else {
                    std::cout << start << "," << end << "c\n";
                }

                if (is_overlap && (opts.mode == OutputMode::EdShowOverlap || opts.mode == OutputMode::EdAll)) {
                    std::string label1 = opts.labels.size() > 0 ? opts.labels[0] : opts.file_mine;
                    std::string label3 = opts.labels.size() > 2 ? opts.labels[2] : opts.file_yours;

                    std::cout << "<<<<<<< " << label1 << "\n";
                    for (const auto& l : c.mine_lines) std::cout << l << "\n";
                    std::cout << "=======\n";
                    for (const auto& l : c.yours_lines) std::cout << l << "\n";
                    std::cout << ">>>>>>> " << label3 << "\n";
                } else {
                    for (const auto& l : c.yours_lines) std::cout << l << "\n";
                }
                std::cout << ".\n";
            }
            return conflict_present;
        }
    };

    class ArgParser {
    public:
        static bool parse(int argc, char* argv[], Options& opts) {
            std::vector<std::string> positional;
            for (int i = 1; i < argc; ++i) {
                std::string arg = argv[i];
                if (arg == "-h" || arg == "--help") {
                    opts.show_help = true;
                    return true;
                } else if (arg == "-v" || arg == "--version") {
                    opts.show_version = true;
                    return true;
                } else if (arg == "-m" || arg == "--merge") {
                    opts.mode = OutputMode::Merge;
                } else if (arg == "-A" || arg == "--show-all") {
                    opts.mode = OutputMode::Merge;
                    opts.show_base_in_conflict = true;
                } else if (arg == "-e" || arg == "--ed") {
                    opts.mode = OutputMode::EdAll;
                } else if (arg == "-E" || arg == "--show-overlap") {
                    opts.mode = OutputMode::EdShowOverlap;
                } else if (arg == "-x" || arg == "--overlap-only") {
                    opts.mode = OutputMode::EdOverlap;
                } else if (arg == "-3" || arg == "--easy-only") {
                    opts.mode = OutputMode::EdNonOverlap;
                } else if (arg == "-T" || arg == "--initial-tab") {
                    opts.initial_tab = true;
                } else if (arg == "-L" || arg == "--label") {
                    if (i + 1 < argc) {
                        opts.labels.push_back(argv[++i]);
                    } else {
                        std::cerr << "diff3: option requires an argument -- 'L'\n";
                        return false;
                    }
                } else if (arg.rfind("-L", 0) == 0 && arg.length() > 2) {
                    opts.labels.push_back(arg.substr(2));
                } else if (arg.rfind("--label=", 0) == 0) {
                    opts.labels.push_back(arg.substr(8));
                } else if (arg.length() > 1 && arg[0] == '-' && arg != "-") {
                    // Clustered short flags
                    for (size_t j = 1; j < arg.length(); ++j) {
                        switch (arg[j]) {
                            case 'm': opts.mode = OutputMode::Merge; break;
                            case 'A': opts.mode = OutputMode::Merge; opts.show_base_in_conflict = true; break;
                            case 'e': opts.mode = OutputMode::EdAll; break;
                            case 'E': opts.mode = OutputMode::EdShowOverlap; break;
                            case 'x': opts.mode = OutputMode::EdOverlap; break;
                            case '3': opts.mode = OutputMode::EdNonOverlap; break;
                            case 'T': opts.initial_tab = true; break;
                            default:
                                std::cerr << "diff3: invalid option -- '" << arg[j] << "'\n";
                                return false;
                        }
                    }
                } else {
                    positional.push_back(arg);
                }
            }

            if (opts.show_help || opts.show_version) return true;

            if (positional.size() < 3) {
                std::cerr << "diff3: missing operand\nTry 'diff3 --help' for more information.\n";
                return false;
            }
            if (positional.size() > 3) {
                std::cerr << "diff3: extra operand '" << positional[3] << "'\nTry 'diff3 --help' for more information.\n";
                return false;
            }

            opts.file_mine = positional[0];
            opts.file_old = positional[1];
            opts.file_yours = positional[2];

            // Verify stdin is referenced at most once
            int stdin_count = (opts.file_mine == "-" ? 1 : 0) +
                              (opts.file_old == "-" ? 1 : 0) +
                              (opts.file_yours == "-" ? 1 : 0);
            if (stdin_count > 1) {
                std::cerr << "diff3: standard input cannot be used for multiple operands\n";
                return false;
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
            std::cout << "diff3 version 3.6.2\n"
                      << "Copyright (C) 2026, Roberto J Dohnert.\n";
        }

        static void print_help() {
            std::cout << R"(diff3(1)                   CrossShell for UNIX Reference Manual                   diff3(1)

    NAME
        diff3 - compare three files line by line

    SYNOPSIS
        diff3 [OPTION]... MYFILE OLDFILE YOURFILE

    DESCRIPTION
        Compare three files line by line.

        An operand of '-' refers to standard input (at most one file may be '-').

    OPTIONS
        -m, --merge
            Output merged file with conflict brackets.
        -A, --show-all
            Output merged file, bracketing all conflicts with base.
        -e, --ed
            Output an ed script incorporating changes from OLDFILE to YOURFILE.
        -E, --show-overlap
            Like -e, but bracket overlapping changes.
        -3, --easy-only
            Like -e, but incorporate only nonoverlapping changes.
        -x, --overlap-only
            Like -e, but incorporate only overlapping changes.
        -T, --initial-tab
            Output a tab before normal diff3 output lines.
        -L, --label=LABEL
            Use LABEL instead of file name (can be repeated up to 3 times).
        -h, --help
            Display this help and exit.
        -v, --version
            Output version information and exit.

    EXAMPLES
        diff3 mine.txt base.txt yours.txt
            Default 3-way difference report.

        diff3 -m mine.txt base.txt yours.txt
            3-way merge to standard output with conflict markers.

        diff3 -m -L "Local" -L "Ancestor" -L "Remote" local.cpp base.cpp remote.cpp
            Merge with custom labels.

    CrossShell for UNIX                                                    diff3(1)
)";
        }
    };

    // =========================================================================
    // Main Application Coordinator
    // =========================================================================
    class Diff3App {
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
                return static_cast<int>(ExitCode::SuccessNoConflicts);
            }

            if (opts.show_version) {
                HelpSystem::print_version();
                return static_cast<int>(ExitCode::SuccessNoConflicts);
            }

            std::vector<std::string> lines_mine, lines_base, lines_yours;

            if (!LineReader::read_file(opts.file_mine, lines_mine, opts.strip_trailing_cr)) return static_cast<int>(ExitCode::Error);
            if (!LineReader::read_file(opts.file_old, lines_base, opts.strip_trailing_cr)) return static_cast<int>(ExitCode::Error);
            if (!LineReader::read_file(opts.file_yours, lines_yours, opts.strip_trailing_cr)) return static_cast<int>(ExitCode::Error);

            auto chunks = Diff3Engine::align(lines_mine, lines_base, lines_yours);

            std::unique_ptr<IOutputFormatter> formatter;
            if (opts.mode == OutputMode::Merge) {
                formatter = std::make_unique<MergeFormatter>();
            } else if (opts.mode == OutputMode::Traditional) {
                formatter = std::make_unique<TraditionalFormatter>();
            } else {
                formatter = std::make_unique<EdFormatter>();
            }

            bool has_conflicts = formatter->format(chunks, opts);

            return has_conflicts ? static_cast<int>(ExitCode::ConflictsFound)
                                 : static_cast<int>(ExitCode::SuccessNoConflicts);
        }
    };
}

int main(int argc, char* argv[]) {
    return Diff3Util::Diff3App::run(argc, argv);
}