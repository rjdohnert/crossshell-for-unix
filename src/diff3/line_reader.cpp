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

#include "line_reader.hpp"
#include <iostream>
#include <fstream>
#include <memory>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace Diff3Util {

bool LineReader::read_file(const std::string& path, std::vector<std::string>& lines, bool strip_cr) {
    std::istream* stream_ptr = nullptr;
    std::unique_ptr<std::ifstream> file_stream;

    if (path == "-") {
#ifdef _WIN32
        if (_setmode(_fileno(stdin), _O_BINARY) == -1) return false;
#endif
        stream_ptr = &std::cin;
    } else {
        file_stream = std::make_unique<std::ifstream>(path, std::ios::binary);
        if (!file_stream->is_open()) {
            std::cerr << "diff3: " << path << ": No such file or directory\n";
            return false;
        }
        stream_ptr = file_stream.get();
    }

    std::string line;
    while (std::getline(*stream_ptr, line)) {
        if (strip_cr && !line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }
    return true;
}

} // namespace Diff3Util
