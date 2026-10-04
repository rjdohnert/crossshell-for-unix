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

#define _CRT_SECURE_NO_WARNINGS
#include "string_utils.hpp"
#include <cerrno>
#include <cstdlib>
#include <limits>

namespace AwkSafe {
    bool ParseInteger(const std::string& text, int& value) {
        if (text.empty()) return false;
        char* end = nullptr;
        errno = 0;
        long parsed = std::strtol(text.c_str(), &end, 10);
        if (errno != 0 || end == text.c_str() || *end != '\0') return false;
        if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) return false;
        value = static_cast<int>(parsed);
        return true;
    }

    bool CompileRegex(const std::string& pattern, std::regex& re, std::regex::flag_type flags) {
        if (pattern.size() > 4096) return false;
        try {
            re = std::regex(pattern, flags);
            return true;
        } catch (const std::regex_error&) {
            return false;
        }
    }
}

std::string StringUtils::JsonEscape(const std::string& value) {
    std::string out;
    for (unsigned char ch : value) {
        if (ch == '"') out += "\\\"";
        else if (ch == '\\') out += "\\\\";
        else if (ch == '\n') out += "\\n";
        else if (ch == '\r') out += "\\r";
        else if (ch == '\t') out += "\\t";
        else if (ch < 0x20) out += ' ';
        else out += static_cast<char>(ch);
    }
    return out;
}

#ifdef _WIN32
std::string StringUtils::WideUtf8(const wchar_t* value) {
    if (!value) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string out(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), size, nullptr, nullptr);
    return out;
}

std::string StringUtils::VariantJson(const VARIANT& value) {
    VARIANT normalized;
    VariantInit(&normalized);
    if (FAILED(VariantChangeType(&normalized, const_cast<VARIANT*>(&value), 0, VT_BSTR))) {
        return "null";
    }
    std::string result = "\"" + JsonEscape(WideUtf8(normalized.bstrVal)) + "\"";
    VariantClear(&normalized);
    return result;
}
#endif
