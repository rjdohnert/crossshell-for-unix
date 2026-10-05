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

#include "engine.hpp"
#include "event_reader.hpp"
#include <iostream>
#include <sstream>
#include <locale>
#include <cstdio>
#include <utility>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace dmesg {

std::string DmesgEngine::utf8FromWide(const std::wstring& value) {
    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

DmesgEngine::DmesgEngine(DmesgOptions opts) : options(std::move(opts)) {}

int DmesgEngine::execute() {
    std::wcout.imbue(std::locale(""));
    bool colorsEnabled = options.useColor && AnsiColor::enableVirtualTerminal();

    std::wostringstream captured;
    std::wstreambuf* oldOutput = nullptr;
    if (options.outputFormat || !options.pipeCommand.empty()) {
        oldOutput = std::wcout.rdbuf(captured.rdbuf());
    }

    EventLogReader::printKernelEvents(options, colorsEnabled);

    if (oldOutput) {
        std::wcout.rdbuf(oldOutput);
        std::wstring data = captured.str();
        std::wstring text = options.outputFormat == 1 ? L"{\"output\":\"" + data + L"\"}\n"
                          : options.outputFormat == 2 ? L"\"output\"\n\"" + data + L"\"\n"
                          : L"OUTPUT\n------\n" + data;

        if (!options.pipeCommand.empty()) {
            FILE* pipe = POPEN(options.pipeCommand.c_str(), "w");
            if (!pipe) return 1;
            std::string narrow = utf8FromWide(text);
            std::fwrite(narrow.data(), 1, narrow.size(), pipe);
            PCLOSE(pipe);
        } else {
            std::wcout << text;
        }
    }
    return 0;
}

} // namespace dmesg
