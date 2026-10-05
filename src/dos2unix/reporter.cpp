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

#include "reporter.hpp"
#include <iostream>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace dos2unix {

std::string Dos2UnixReporter::toUtf8(const std::wstring& text) {
#ifdef _WIN32
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, NULL, NULL);
    return result;
#else
    return std::string(text.begin(), text.end());
#endif
}

int Dos2UnixReporter::dispatch(const std::vector<std::pair<std::wstring, std::wstring>>& conversions,
                               int format, const std::wstring& pipeCommand) {
    if (format == 0 && pipeCommand.empty()) return 0;

    std::wstring text;
    if (format == 1) {
        text = L"{\"conversions\":[";
        for (size_t i = 0; i < conversions.size(); ++i) {
            if (i > 0) text += L",";
            text += L"{\"in\":\"" + conversions[i].first + L"\",\"out\":\"" + conversions[i].second + L"\"}";
        }
        text += L"]}\n";
    } else if (format == 2) {
        text = L"input,output\n";
        for (const auto& c : conversions) {
            text += L"\"" + c.first + L"\",\"" + c.second + L"\"\n";
        }
    } else if (format == 3) {
        text = L"INPUT\tOUTPUT\n--------------------\n";
        for (const auto& c : conversions) {
            text += c.first + L"\t" + c.second + L"\n";
        }
    }

    if (!pipeCommand.empty()) {
#ifdef _WIN32
        FILE* pipe = _wpopen(pipeCommand.c_str(), L"w");
        if (!pipe) return 1;
        std::string utf8 = toUtf8(text);
        std::fwrite(utf8.data(), 1, utf8.size(), pipe);
        _pclose(pipe);
#endif
    } else {
        std::wcout << text;
    }
    return 0;
}

} // namespace dos2unix
