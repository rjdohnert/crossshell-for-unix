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

#include "value.hpp"
#include <cctype>
#include <iomanip>
#include <sstream>

std::string Value::to_string() const {
    if (type == NIL) return "";
    if (type == BOOL) return std::get<bool>(data) ? "true" : "false";
    if (type == NUMBER) {
        double d = std::get<double>(data);
        if (d == static_cast<long long>(d)) return std::to_string(static_cast<long long>(d));
        std::ostringstream ss;
        ss << std::setprecision(10) << d;
        return ss.str();
    }
    if (type == STRING) return std::get<std::string>(data);
    if (type == ARRAY) {
        std::string out = "[";
        const auto& arr = std::get<Array>(data);
        for (size_t i = 0; i < arr.size(); ++i) {
            out += arr[i].to_string() + (i + 1 < arr.size() ? ", " : "");
        }
        return out + "]";
    }
    if (type == OBJECT) {
        std::string out = "{";
        const auto& obj = std::get<Object>(data);
        size_t i = 0;
        for (const auto& [k, v] : obj) {
            out += "\"" + k + "\": " + v.to_string() + (++i < obj.size() ? ", " : "");
        }
        return out + "}";
    }
    return "";
}

double Value::to_number() const {
    if (type == NUMBER) return std::get<double>(data);
    if (type == BOOL) return std::get<bool>(data) ? 1.0 : 0.0;
    if (type == STRING) {
        try { return std::stod(std::get<std::string>(data)); } catch (...) { return 0.0; }
    }
    return 0.0;
}

bool Value::to_bool() const {
    if (type == NIL) return false;
    if (type == BOOL) return std::get<bool>(data);
    if (type == NUMBER) return std::get<double>(data) != 0.0;
    if (type == STRING) return !std::get<std::string>(data).empty() && std::get<std::string>(data) != "0";
    if (type == ARRAY) return !std::get<Array>(data).empty();
    if (type == OBJECT) return !std::get<Object>(data).empty();
    return true;
}

Value Value::get_path(const std::string& key) const {
    if (type == OBJECT) {
        const auto& obj = std::get<Object>(data);
        auto it = obj.find(key);
        if (it != obj.end()) return it->second;
    } else if (type == ARRAY) {
        try {
            size_t idx = std::stoul(key);
            const auto& arr = std::get<Array>(data);
            if (idx < arr.size()) return arr[idx];
        } catch (...) {}
    }
    return Value();
}

MiniJsonParser::MiniJsonParser(std::string s) : src(std::move(s)), pos(0) {}

void MiniJsonParser::skip_ws() {
    while (pos < src.size() && (std::isspace(static_cast<unsigned char>(src[pos])))) pos++;
}

char MiniJsonParser::peek() {
    skip_ws();
    return pos < src.size() ? src[pos] : '\0';
}

char MiniJsonParser::get() {
    skip_ws();
    return pos < src.size() ? src[pos++] : '\0';
}

std::string MiniJsonParser::parse_string() {
    get(); // skip opening "
    std::string s;
    while (pos < src.size()) {
        char c = src[pos++];
        if (c == '"') break;
        if (c == '\\' && pos < src.size()) {
            char esc = src[pos++];
            if (esc == 'n') s += '\n';
            else if (esc == 't') s += '\t';
            else if (esc == 'r') s += '\r';
            else s += esc;
        } else {
            s += c;
        }
    }
    return s;
}

Value MiniJsonParser::parse_number() {
    size_t start = pos;
    if (src[pos] == '-') pos++;
    while (pos < src.size() && (std::isdigit(static_cast<unsigned char>(src[pos])) || src[pos] == '.' || src[pos] == 'e' || src[pos] == 'E' || src[pos] == '+' || src[pos] == '-')) {
        pos++;
    }
    try {
        return Value(std::stod(src.substr(start, pos - start)));
    } catch (...) {
        return Value(0.0);
    }
}

Value MiniJsonParser::parse_value() {
    char c = peek();
    if (c == '{') {
        get();
        Object obj;
        while (peek() != '}' && peek() != '\0') {
            if (peek() == ',') { get(); continue; }
            std::string k = parse_string();
            if (peek() == ':') get();
            obj[k] = parse_value();
        }
        if (peek() == '}') get();
        return Value(obj);
    } else if (c == '[') {
        get();
        Array arr;
        while (peek() != ']' && peek() != '\0') {
            if (peek() == ',') { get(); continue; }
            arr.push_back(parse_value());
        }
        if (peek() == ']') get();
        return Value(arr);
    } else if (c == '"') {
        return Value(parse_string());
    } else if (std::isdigit(static_cast<unsigned char>(c)) || c == '-') {
        return parse_number();
    } else if (c == 't' || c == 'f') {
        std::string b;
        while (std::isalpha(static_cast<unsigned char>(peek()))) b += get();
        return Value(b == "true");
    } else if (c == 'n') {
        for (int i = 0; i < 4; ++i) get();
        return Value();
    }
    return Value();
}
