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

#include "data_transformer.hpp"
#include <algorithm>
#include <cctype>

void DataTransformer::Transform(std::vector<char>& buffer, DWORD& bytes_read, const DdOptions& opt) {
    // conv=sync padding
    if (opt.conv_sync && bytes_read < opt.ibs) {
        std::fill(buffer.begin() + bytes_read, buffer.begin() + opt.ibs, 0);
        bytes_read = static_cast<DWORD>(opt.ibs);
    }

    // conv=ucase / conv=lcase
    if (opt.conv_ucase) {
        for (DWORD i = 0; i < bytes_read; ++i) {
            buffer[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(buffer[i])));
        }
    } else if (opt.conv_lcase) {
        for (DWORD i = 0; i < bytes_read; ++i) {
            buffer[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(buffer[i])));
        }
    }

    // conv=swab
    if (opt.conv_swab) {
        for (DWORD i = 0; i + 1 < bytes_read; i += 2) {
            std::swap(buffer[i], buffer[i + 1]);
        }
    }
}
