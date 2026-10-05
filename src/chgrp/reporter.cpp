/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
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

#include "reporter.hpp"
#include <iostream>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

std::string ChgrpReporter::ToUtf8(const std::wstring& value) {
#ifdef _WIN32
    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
#else
    return std::string(value.begin(), value.end());
#endif
}

void ChgrpReporter::OutputResults(bool success, OutputFormat format, const std::wstring& pipeCommand) {
    if (format == OutputFormat::Default && pipeCommand.empty()) return;

    std::wstring text;
    if (format == OutputFormat::Json) {
        text = L"{\"status\":\"" + std::wstring(success ? L"success" : L"failure") + L"\"}\n";
    } else if (format == OutputFormat::Csv) {
        text = L"status\n" + std::wstring(success ? L"success\n" : L"failure\n");
    } else {
        text = L"STATUS\n" + std::wstring(success ? L"success\n" : L"failure\n");
    }

    if (!pipeCommand.empty()) {
#ifdef _WIN32
        FILE* pipe = _wpopen(pipeCommand.c_str(), L"w");
        if (pipe) {
            std::string narrow = ToUtf8(text);
            fwrite(narrow.data(), 1, narrow.size(), pipe);
            _pclose(pipe);
        }
#else
        std::string cmd = ToUtf8(pipeCommand);
        FILE* pipe = popen(cmd.c_str(), "w");
        if (pipe) {
            std::string narrow = ToUtf8(text);
            fwrite(narrow.data(), 1, narrow.size(), pipe);
            pclose(pipe);
        }
#endif
    } else {
        std::wcout << text;
    }
}
