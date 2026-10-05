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

#include "diff3_engine.hpp"
#include "myers_diff.hpp"

namespace Diff3Util {

std::vector<Diff3Chunk> Diff3Engine::align(const std::vector<std::string>& mine,
                                           const std::vector<std::string>& base,
                                           const std::vector<std::string>& yours) {
    auto edits_mine = MyersDiff::compute(base, mine);
    auto edits_yours = MyersDiff::compute(base, yours);

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

} // namespace Diff3Util
