/*
 * Copyright (c) 2025, R. J. Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "formatter.hpp"
#include <iostream>

namespace Diff3Util {

void TraditionalFormatter::print_range(int file_no, size_t start, size_t end) {
    std::cout << file_no << ":";
    if (start > end) {
        std::cout << (start - 1) << "a\n";
    } else if (start == end) {
        std::cout << start << "c\n";
    } else {
        std::cout << start << "," << end << "c\n";
    }
}

bool TraditionalFormatter::format(const std::vector<Diff3Chunk>& chunks, const Options& opts) {
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

bool MergeFormatter::format(const std::vector<Diff3Chunk>& chunks, const Options& opts) {
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

bool EdFormatter::format(const std::vector<Diff3Chunk>& chunks, const Options& opts) {
    bool conflict_present = false;
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

} // namespace Diff3Util
