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

#include "myers_diff.hpp"
#include <algorithm>

namespace DiffUtil {

std::vector<EditItem> MyersDiff::compute(const std::vector<std::string>& a, const std::vector<std::string>& b) {
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

std::vector<EditItem> MyersDiff::backtrack(const std::vector<std::vector<int>>& trace,
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

} // namespace DiffUtil
