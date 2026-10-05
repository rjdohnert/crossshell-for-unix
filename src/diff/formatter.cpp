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
#include <algorithm>

namespace DiffUtil {

void BriefFormatter::format(const std::vector<EditItem>&,
                            const std::vector<std::string>&,
                            const std::vector<std::string>&,
                            const Options& opts) {
    std::cout << "Files " << opts.file1 << " and " << opts.file2 << " differ\n";
}

void NormalFormatter::print_range(size_t start, size_t end) {
    if (start == end) {
        std::cout << start;
    } else {
        std::cout << start << "," << end;
    }
}

void NormalFormatter::format(const std::vector<EditItem>& edits,
                            const std::vector<std::string>& lines1,
                            const std::vector<std::string>& lines2,
                            const Options&) {
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

void UnifiedFormatter::format(const std::vector<EditItem>& edits,
                              const std::vector<std::string>& lines1,
                              const std::vector<std::string>& lines2,
                              const Options& opts) {
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

} // namespace DiffUtil
