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

#include "output_reporter.hpp"
#include <iostream>
#include <cstdio>

std::string A2PdfOutput::format(const std::string& value, int mode) {
    if (mode == 1) return "{\"message\":\"" + escape(value) + "\"}\n";
    if (mode == 2) return "\"message\"\n\"" + csv(value) + "\"\n";
    return "MESSAGE\n-------\n" + value;
}

void A2PdfOutput::write(const std::string& value, int mode, const std::string& pipe) {
    std::string text = format(value, mode);
    if (!pipe.empty()) {
        FILE* p = _popen(pipe.c_str(), "w");
        if (p) {
            fwrite(text.data(), 1, text.size(), p);
            _pclose(p);
        }
    } else {
        std::cout << text;
    }
}

std::string A2PdfOutput::escape(const std::string& value) {
    std::string out;
    for (char c : value) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') out += "\\n";
        else if (c != '\r') out += c;
    }
    return out;
}

std::string A2PdfOutput::csv(const std::string& value) {
    std::string out;
    for (char c : value) {
        out += (c == '"') ? "\"\"" : std::string(1, c);
    }
    return out;
}
