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

#include "range_parser.hpp"
#include <sstream>
#include <algorithm>

bool RangeParser::ParseList(const std::string& list_str, Selection& sel) {
    std::stringstream ss(list_str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (item.empty()) return false;

        if (std::count(item.begin(), item.end(), '-') > 1) {
            return false;
        }

        size_t dash = item.find('-');
        if (dash == std::string::npos) {
            try {
                int val = std::stoi(item);
                if (val <= 0) return false;
                sel.AddRange(val, val);
            } catch (...) {
                return false;
            }
        } else {
            std::string start_str = item.substr(0, dash);
            std::string end_str = item.substr(dash + 1);
            int start = 1;
            int end = -1;

            if (!start_str.empty()) {
                try {
                    start = std::stoi(start_str);
                    if (start <= 0) return false;
                } catch (...) {
                    return false;
                }
            }
            if (!end_str.empty()) {
                try {
                    end = std::stoi(end_str);
                    if (end <= 0) return false;
                } catch (...) {
                    return false;
                }
            }

            if (start != -1 && end != -1 && start > end) {
                return false;
            }
            sel.AddRange(start, end);
        }
    }
    return !sel.ranges.empty();
}
