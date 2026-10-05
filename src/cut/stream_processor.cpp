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

#include "stream_processor.hpp"
#include <vector>

void StreamProcessor::ProcessLine(const std::string& line, const CutOptions& opts, std::ostream& out) {
    char delim_char = opts.delimiter.empty() ? '\t' : opts.delimiter[0];
    std::string effective_out_delim = opts.output_delimiter.empty() ? std::string(1, delim_char) : opts.output_delimiter;

    if (opts.mode == CutMode::FIELDS) {
        size_t pos = line.find(delim_char);
        if (pos == std::string::npos) {
            if (!opts.only_delimited) {
                out << line << "\n";
            }
            return;
        }

        std::vector<std::string> fields;
        size_t start = 0;
        size_t end = line.find(delim_char);
        while (end != std::string::npos) {
            fields.push_back(line.substr(start, end - start));
            start = end + 1;
            end = line.find(delim_char, start);
        }
        fields.push_back(line.substr(start));

        bool first = true;
        for (size_t idx = 0; idx < fields.size(); ++idx) {
            int field_idx = static_cast<int>(idx + 1);
            bool selected = opts.selection.IsSelected(field_idx);
            if (opts.complement) selected = !selected;

            if (selected) {
                if (!first) out << effective_out_delim;
                out << fields[idx];
                first = false;
            }
        }
        out << "\n";
    } else {
        // BYTES or CHARACTERS mode
        std::string result;
        int total_items = static_cast<int>(line.size());
        for (int i = 1; i <= total_items; ++i) {
            bool selected = opts.selection.IsSelected(i);
            if (opts.complement) selected = !selected;
            if (selected) {
                result += line[i - 1];
            }
        }
        out << result << "\n";
    }
}

void StreamProcessor::ProcessStream(std::istream& in, const CutOptions& opts, std::ostream& out) {
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        ProcessLine(line, opts, out);
    }
}
