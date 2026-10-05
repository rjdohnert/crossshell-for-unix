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

namespace dig {

std::wstring DigReporter::EscapeJson(const std::wstring& src) {
    std::wstring out;
    for (wchar_t ch : src) {
        if (ch == L'\\' || ch == L'"') {
            out.push_back(L'\\');
        }
        out.push_back(ch);
    }
    return out;
}

void DigReporter::SendToPipe(const std::wstring& pipeCmd, const std::wstring& text) {
    FILE* pipe = _wpopen(pipeCmd.c_str(), L"w");
    if (!pipe) {
        std::wcerr << L"dig: failed to open pipe: " << pipeCmd << L"\n";
        return;
    }

    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size > 0) {
        std::string narrow(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(), size, nullptr, nullptr);
        fwrite(narrow.data(), 1, narrow.size(), pipe);
    }
    _pclose(pipe);
}

void DigReporter::EmitOutput(const DigOptions& opts, const std::vector<DnsAnswerItem>& answers) {
    if (opts.format == OutputFormat::Standard && opts.pipeCommand.empty()) {
        if (!opts.shortOutput) {
            std::wcout << L"; <<>> dig v1.0.0 <<>> " << opts.name << L" " << opts.queryTypeName << L"\n";
            std::wcout << L";; ANSWER SECTION:\n";
        }

        if (answers.empty() && !opts.shortOutput) {
            std::wcout << L"; no matching answer records\n";
        }

        for (const auto& ans : answers) {
            if (opts.shortOutput) {
                std::wcout << ans.data << L"\n";
            } else {
                std::wcout << opts.name << L"\t" << ans.ttl << L"\tIN\t" << ans.typeName << L"\t" << ans.data << L"\n";
            }
        }
        return;
    }

    std::wstring text;
    if (opts.format == OutputFormat::Json) {
        text = L"{\"name\":\"" + opts.name + L"\",\"type\":\"" + opts.queryTypeName + L"\",\"answers\":[";
        for (size_t i = 0; i < answers.size(); ++i) {
            if (i > 0) text += L",";
            text += L"\"" + EscapeJson(answers[i].data) + L"\"";
        }
        text += L"]}\n";
    } else if (opts.format == OutputFormat::Csv) {
        text = L"name,type,answer\n";
        for (const auto& ans : answers) {
            text += opts.name + L"," + ans.typeName + L"," + ans.data + L"\n";
        }
    } else {
        text = L"NAME\tTYPE\tANSWER\n";
        for (const auto& ans : answers) {
            text += opts.name + L"\t" + ans.typeName + L"\t" + ans.data + L"\n";
        }
    }

    if (!opts.pipeCommand.empty()) {
        SendToPipe(opts.pipeCommand, text);
    } else {
        std::wcout << text;
    }
}

} // namespace dig
