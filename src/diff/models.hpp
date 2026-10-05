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

#ifndef DIFF_MODELS_HPP
#define DIFF_MODELS_HPP

#include <string>
#include <vector>
#include <cstddef>

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

} // namespace DiffUtil

#endif // DIFF_MODELS_HPP
