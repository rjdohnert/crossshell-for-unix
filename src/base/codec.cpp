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

#include "codec.hpp"
#include <cctype>
#include <stdexcept>

const char Base64Codec::ENCODE_TABLE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Codec::encode(const std::vector<uint8_t>& data) {
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i < data.size()) {
        uint32_t octet_a = (i < data.size()) ? data[i++] : 0;
        uint32_t octet_b = (i < data.size()) ? data[i++] : 0;
        uint32_t octet_c = (i < data.size()) ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        result.push_back(ENCODE_TABLE[(triple >> 18) & 0x3F]);
        result.push_back(ENCODE_TABLE[(triple >> 12) & 0x3F]);
        result.push_back((i > data.size() + 1) ? '=' : ENCODE_TABLE[(triple >> 6) & 0x3F]);
        result.push_back((i > data.size()) ? '=' : ENCODE_TABLE[triple & 0x3F]);
    }
    return result;
}

std::vector<uint8_t> Base64Codec::decode(const std::string& input, bool ignoreGarbage) {
    std::vector<uint8_t> result;
    std::vector<int> table(256, -1);
    for (int i = 0; i < 64; ++i) table[static_cast<unsigned char>(ENCODE_TABLE[i])] = i;

    uint32_t val = 0;
    int valb = -8;
    for (unsigned char c : input) {
        if (c == '=') break;
        int d = table[c];
        if (d == -1) {
            if (ignoreGarbage || std::isspace(c)) continue;
            throw std::runtime_error("invalid character in input");
        }
        val = (val << 6) | d;
        valb += 6;
        if (valb >= 0) {
            result.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return result;
}

const char Base32Codec::ENCODE_TABLE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

std::string Base32Codec::encode(const std::vector<uint8_t>& data) {
    std::string result;
    result.reserve(((data.size() + 4) / 5) * 8);

    size_t i = 0;
    while (i < data.size()) {
        uint64_t buffer = 0;
        int bytes = 0;
        for (int k = 0; k < 5; ++k) {
            buffer <<= 8;
            if (i < data.size()) {
                buffer |= data[i++];
                bytes++;
            }
        }

        int pad = 0;
        if (bytes == 1) pad = 6;
        else if (bytes == 2) pad = 4;
        else if (bytes == 3) pad = 3;
        else if (bytes == 4) pad = 1;

        for (int bit = 35; bit >= 0; bit -= 5) {
            result.push_back(ENCODE_TABLE[(buffer >> bit) & 0x1F]);
        }

        for (int p = 0; p < pad; ++p) {
            result[result.size() - 1 - p] = '=';
        }
    }
    return result;
}

std::vector<uint8_t> Base32Codec::decode(const std::string& input, bool ignoreGarbage) {
    std::vector<uint8_t> result;
    std::vector<int> table(256, -1);
    for (int i = 0; i < 32; ++i) table[static_cast<unsigned char>(ENCODE_TABLE[i])] = i;

    uint64_t val = 0;
    int valb = -8;
    for (unsigned char c : input) {
        if (c == '=') break;
        int d = table[c];
        if (d == -1) {
            if (ignoreGarbage || std::isspace(c)) continue;
            throw std::runtime_error("invalid character in input");
        }
        val = (val << 5) | d;
        valb += 5;
        if (valb >= 0) {
            result.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return result;
}
