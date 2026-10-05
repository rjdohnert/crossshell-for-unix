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

#include "reporter.hpp"
#include <cstdio>
#include <iostream>

std::string ChattrReporter::ToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
#ifdef _WIN32
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
#else
    return std::string(text.begin(), text.end());
#endif
}

void ChattrReporter::OutputResults(const std::wstring& outputData, bool success, OutputFormat format, const std::wstring& pipeCommand) {
    std::wstring text;
    if (format == OutputFormat::Json) {
        text = L"{\"status\":\"" + std::wstring(success ? L"success" : L"failure") + L"\",\"output\":\"" + outputData + L"\"}\n";
    } else if (format == OutputFormat::Csv) {
        text = L"status,output\n" + std::wstring(success ? L"success,\"" : L"failure,\"") + outputData + L"\"\n";
    } else if (format == OutputFormat::Table) {
        text = L"STATUS\tOUTPUT\n" + std::wstring(success ? L"success\t" : L"failure\t") + outputData + L"\n";
    } else {
        text = outputData;
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
