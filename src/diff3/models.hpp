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

#ifndef DIFF3_MODELS_HPP
#define DIFF3_MODELS_HPP

#include <string>
#include <vector>
#include <cstddef>

namespace Diff3Util {

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

} // namespace Diff3Util

#endif // DIFF3_MODELS_HPP
