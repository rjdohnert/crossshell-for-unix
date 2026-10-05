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

#include "converter.hpp"
#include "reporter.hpp"
#include "timestamp_helper.hpp"

#include <fstream>
#include <iostream>
#include <vector>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

namespace dos2unix {

bool LineEndingConverter::isBinary(const std::vector<char>& buffer, size_t bytesRead) {
    for (size_t i = 0; i < bytesRead; ++i) {
        if (buffer[i] == '\0') return true;
    }
    return false;
}

bool LineEndingConverter::convertStream(std::istream& in, std::ostream& out) const {
    std::vector<char> inBuf(65536);
    std::vector<char> outBuf(65536);
    size_t outPos = 0;

    auto flushOut = [&]() {
        if (outPos > 0) {
            out.write(outBuf.data(), outPos);
            outPos = 0;
        }
    };

    auto emitByte = [&](char b) {
        outBuf[outPos++] = b;
        if (outPos == outBuf.size()) flushOut();
    };

    if (options.mode == LineEndingMode::DosToUnix) {
        char ch = 0;
        while (in.get(ch)) {
            if (ch == '\r') {
                int next = in.peek();
                if (next == '\n') {
                    continue;
                }
            }
            emitByte(ch);
        }
    } else {
        char ch = 0;
        while (in.get(ch)) {
            if (ch == '\n') {
                emitByte('\r');
                emitByte('\n');
            } else if (ch == '\r') {
                int next = in.peek();
                if (next == '\n') {
                    emitByte('\r');
                    emitByte('\n');
                    in.get();
                } else {
                    emitByte('\r');
                }
            } else {
                emitByte(ch);
            }
        }
    }

    flushOut();
    return true;
}

LineEndingConverter::LineEndingConverter(Dos2UnixOptions opts) : options(std::move(opts)) {}

int LineEndingConverter::execute() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    if (options.filePairs.empty()) {
        convertStream(std::cin, std::cout);
        return 0;
    }

    std::vector<std::pair<std::wstring, std::wstring>> converted;
    bool allOk = true;

    for (const auto& pair : options.filePairs) {
        const std::wstring& inFile = pair.first;
        const std::wstring& outFile = pair.second;

        if (inFile == L"-") {
            convertStream(std::cin, std::cout);
            converted.emplace_back(L"STDIN", L"STDOUT");
            continue;
        }

        FileTimeInfo times = options.preserveDate ? FileTimestampHelper::getTimestamps(inFile) : FileTimeInfo{};

        std::ifstream in(inFile, std::ios::binary);
        if (!in.is_open()) {
            if (!options.quiet) std::wcerr << L"dos2unix: cannot open " << inFile << L"\n";
            allOk = false;
            continue;
        }

        std::wstring tempOut = outFile + L".tmp_d2u";
        std::ofstream out(tempOut, std::ios::binary);
        if (!out.is_open()) {
            if (!options.quiet) std::wcerr << L"dos2unix: cannot create " << tempOut << L"\n";
            allOk = false;
            continue;
        }

        convertStream(in, out);
        in.close();
        out.close();

#ifdef _WIN32
        if (inFile == outFile) {
            DeleteFileW(inFile.c_str());
        }
        MoveFileExW(tempOut.c_str(), outFile.c_str(), MOVEFILE_REPLACE_EXISTING);
#endif

        if (options.preserveDate && times.valid) {
            FileTimestampHelper::setTimestamps(outFile, times);
        }

        if (options.verbose && !options.quiet) {
            std::wcout << L"converted " << inFile << L" -> " << outFile << L"\n";
        }
        converted.emplace_back(inFile, outFile);
    }

    Dos2UnixReporter::dispatch(converted, options.outputFormat, options.pipeCommand);
    return allOk ? 0 : 1;
}

} // namespace dos2unix
