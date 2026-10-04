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

#include "audit_session.hpp"
#include <cstdio>
#include <utility>

std::string AuditOutputSession::narrow(const std::wstring& value) {
    std::string result;
    for (wchar_t ch : value) result.push_back(static_cast<char>(ch));
    return result;
}

AuditOutputSession::AuditOutputSession(int format, std::wstring pipe)
    : format_(format), pipe_(std::move(pipe)), old_(nullptr) {
    if (format_ || !pipe_.empty()) {
        old_ = std::wcout.rdbuf(capture_.rdbuf());
    }
}

AuditOutputSession::~AuditOutputSession() {
    if (!old_) return;
    std::wcout.rdbuf(old_);
    std::wstring data = capture_.str();
    std::wstring output;
    if (format_ == 1) output = L"[{\"output\":\"" + data + L"\"}]\n";
    else if (format_ == 2) output = L"\"output\"\n\"" + data + L"\"\n";
    else output = L"OUTPUT\n------\n" + data;

    if (!pipe_.empty()) {
        FILE* pipe = _wpopen(pipe_.c_str(), L"w");
        if (pipe) {
            std::string text = narrow(output);
            std::fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
        }
    } else {
        std::wcout << output;
    }
}
