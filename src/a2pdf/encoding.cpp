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

#include "encoding.hpp"
#include <cstdint>

std::u32string Utf8ToUtf32(const std::string& str) {
    std::u32string result;
    size_t i = 0;
    while (i < str.length()) {
        uint32_t cp = 0;
        unsigned char c = str[i];
        if (c < 0x80) {
            cp = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < str.length()) cp = ((c & 0x1F) << 6) | (str[i + 1] & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 < str.length()) cp = ((c & 0x0F) << 12) | ((str[i + 1] & 0x3F) << 6) | (str[i + 2] & 0x3F);
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 < str.length()) cp = ((c & 0x07) << 18) | ((str[i + 1] & 0x3F) << 12) | ((str[i + 2] & 0x3F) << 6) | (str[i + 3] & 0x3F);
            i += 4;
        } else {
            cp = '?';
            i += 1;
        }
        result.push_back(cp);
    }
    return result;
}

// Map Unicode Code Points to PDF/PS WinAnsiEncoding (Windows-1252) Bytes
std::string Utf32ToWinAnsiPdfString(const std::u32string& str32) {
    std::string output = "";
    for (char32_t cp : str32) {
        unsigned char b = '?';
        if (cp >= 0x20 && cp <= 0x7E) {
            if (cp == '(' || cp == ')' || cp == '\\') {
                output += '\\';
                output += static_cast<char>(cp);
                continue;
            }
            b = static_cast<unsigned char>(cp);
        } else if (cp >= 0xA0 && cp <= 0xFF) {
            b = static_cast<unsigned char>(cp); // Direct Latin-1 mapping
        } else {
            // Windows-1252 Extensions Mapping
            switch (cp) {
                case 0x20AC: b = 0x80; break; // Euro €
                case 0x201A: b = 0x82; break; // Single low quote ‚
                case 0x201E: b = 0x84; break; // Double low quote „
                case 0x2026: b = 0x85; break; // Ellipsis …
                case 0x2018: b = 0x91; break; // Left single quote ‘
                case 0x2019: b = 0x92; break; // Right single quote ’
                case 0x201C: b = 0x93; break; // Left double quote “
                case 0x201D: b = 0x94; break; // Right double quote ”
                case 0x2022: b = 0x95; break; // Bullet •
                case 0x2013: b = 0x96; break; // En dash –
                case 0x2014: b = 0x97; break; // Em dash —
                case 0x2122: b = 0x99; break; // Trademark ™
                default:     b = '?';    break; // Fallback for unsupported glyphs
            }
        }
        output += static_cast<char>(b);
    }
    return output;
}
