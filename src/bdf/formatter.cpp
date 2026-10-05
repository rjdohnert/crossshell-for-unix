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
 *
 * CrossShell for UNIX
 */

#include "formatter.hpp"
#include <iomanip>
#include <sstream>

std::wstring SizeFormatter::formatSize(uint64_t bytes, const ProgramOptions& opts) {
    switch (opts.scale) {
        case ScaleUnit::HumanBinary:
            return formatHuman(bytes, 1024.0, { L"B", L"K", L"M", L"G", L"T", L"P", L"E" });
        case ScaleUnit::HumanDecimal:
            return formatHuman(bytes, 1000.0, { L"B", L"k", L"M", L"G", L"T", L"P", L"E" });
        case ScaleUnit::KiloBinary:
            return std::to_wstring((bytes + 1023) / 1024);
        case ScaleUnit::MegaBinary:
            return std::to_wstring((bytes + (1024 * 1024 - 1)) / (1024 * 1024));
        case ScaleUnit::GigaBinary:
            return std::to_wstring((bytes + (1024ULL * 1024 * 1024 - 1)) / (1024ULL * 1024 * 1024));
        case ScaleUnit::CustomBlock: {
            uint64_t blk = opts.customBlockSize > 0 ? opts.customBlockSize : 1;
            return std::to_wstring((bytes + blk - 1) / blk);
        }
        default:
            return std::to_wstring(bytes);
    }
}

std::wstring SizeFormatter::getBlockHeader(const ProgramOptions& opts) {
    switch (opts.scale) {
        case ScaleUnit::HumanBinary:
        case ScaleUnit::HumanDecimal:
            return L"Size";
        case ScaleUnit::KiloBinary:
            return L"1K-blocks";
        case ScaleUnit::MegaBinary:
            return L"1M-blocks";
        case ScaleUnit::GigaBinary:
            return L"1G-blocks";
        case ScaleUnit::CustomBlock:
            return std::to_wstring(opts.customBlockSize) + L"-blocks";
        default:
            return L"Blocks";
    }
}

std::wstring SizeFormatter::formatHuman(uint64_t bytes, double base, const std::vector<std::wstring>& units) {
    if (bytes == 0) return L"0" + units[0];

    double size = static_cast<double>(bytes);
    size_t unitIndex = 0;
    while (size >= base && unitIndex < units.size() - 1) {
        size /= base;
        unitIndex++;
    }

    std::wostringstream oss;
    if (unitIndex == 0) {
        oss << static_cast<uint64_t>(size) << units[unitIndex];
    } else if (size < 10.0) {
        oss << std::fixed << std::setprecision(1) << size << units[unitIndex];
    } else {
        oss << std::fixed << std::setprecision(0) << size << units[unitIndex];
    }
    return oss.str();
}
