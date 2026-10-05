/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
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

#ifndef CUT_MODELS_HPP
#define CUT_MODELS_HPP

#include <string>
#include <vector>

struct Range {
    int start;
    int end; // -1 indicates open-ended (up to the end of the line)
};

class Selection {
public:
    std::vector<Range> ranges;

    void AddRange(int start, int end) {
        ranges.push_back({start, end});
    }

    bool IsSelected(int index) const {
        for (const auto& r : ranges) {
            if (r.end == -1) {
                if (index >= r.start) return true;
            } else {
                if (index >= r.start && index <= r.end) return true;
            }
        }
        return false;
    }
};

enum class CutMode {
    NONE,
    BYTES,
    CHARACTERS,
    FIELDS
};

struct CutOptions {
    CutMode mode = CutMode::NONE;
    std::string list_str;
    Selection selection;
    std::string delimiter = "\t";
    std::string output_delimiter;
    bool only_delimited = false;
    bool complement = false;
    int output_format = 0; // 0=raw, 1=json, 2=csv, 3=table
    std::string pipe_command;
    std::vector<std::string> files;
};

#endif // CUT_MODELS_HPP
