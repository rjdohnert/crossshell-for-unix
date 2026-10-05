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

#ifndef DIFF3_FORMATTER_HPP
#define DIFF3_FORMATTER_HPP

#include "models.hpp"
#include <vector>

namespace Diff3Util {

class IOutputFormatter {
public:
    virtual ~IOutputFormatter() = default;
    virtual bool format(const std::vector<Diff3Chunk>& chunks,
                        const Options& opts) = 0;
};

// Standard traditional diff3 output format
class TraditionalFormatter : public IOutputFormatter {
public:
    bool format(const std::vector<Diff3Chunk>& chunks, const Options& opts) override;

private:
    static void print_range(int file_no, size_t start, size_t end);
};

// 3-Way Merge Formatter (-m, -A, -E)
class MergeFormatter : public IOutputFormatter {
public:
    bool format(const std::vector<Diff3Chunk>& chunks, const Options& opts) override;
};

// Ed Script Formatter (-e, -E, -3, -x)
class EdFormatter : public IOutputFormatter {
public:
    bool format(const std::vector<Diff3Chunk>& chunks, const Options& opts) override;
};

} // namespace Diff3Util

#endif // DIFF3_FORMATTER_HPP
