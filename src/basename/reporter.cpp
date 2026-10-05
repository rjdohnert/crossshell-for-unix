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

int BasenameReporter::report(const std::vector<std::string>& results, int format,
                             bool zeroTerminated, const std::string& pipeCommand) {
    if (format != 0 || !pipeCommand.empty()) {
        std::string text;
        if (format == 1) {
            text = "[\n";
            for (size_t n = 0; n < results.size(); ++n) {
                text += (n ? ",\n" : "") + std::string("{\"value\":\"") + results[n] + "\"}";
            }
            text += "\n]\n";
        } else if (format == 2) {
            text = "\"basename\"\n";
            for (const auto& r : results) {
                text += "\"" + r + "\"\n";
            }
        } else if (format == 3) {
            text = "BASENAME\n--------\n";
            for (const auto& r : results) {
                text += r + "\n";
            }
        }

        if (!pipeCommand.empty()) {
#ifdef _WIN32
            FILE* pipe = _popen(pipeCommand.c_str(), "w");
            if (!pipe) return 1;
            std::fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
#else
            FILE* pipe = popen(pipeCommand.c_str(), "w");
            if (!pipe) return 1;
            std::fwrite(text.data(), 1, text.size(), pipe);
            pclose(pipe);
#endif
        } else {
            std::cout << text;
        }
    } else {
        for (const auto& result : results) {
            std::cout << result;
            if (zeroTerminated) std::cout.put('\0');
            else std::cout.put('\n');
        }
    }
    return 0;
}
