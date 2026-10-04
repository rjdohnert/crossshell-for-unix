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

#ifndef AWK_VALUE_HPP
#define AWK_VALUE_HPP

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

struct Value;
using Object = std::map<std::string, Value>;
using Array  = std::vector<Value>;

struct Value {
    enum Type { NIL, BOOL, NUMBER, STRING, ARRAY, OBJECT } type = NIL;
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data;

    Value() : type(NIL), data(nullptr) {}
    Value(bool b) : type(BOOL), data(b) {}
    Value(double d) : type(NUMBER), data(d) {}
    Value(const std::string& s) : type(STRING), data(s) {}
    Value(const char* s) : type(STRING), data(std::string(s)) {}
    Value(Array a) : type(ARRAY), data(std::move(a)) {}
    Value(Object o) : type(OBJECT), data(std::move(o)) {}

    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] double to_number() const;
    [[nodiscard]] bool to_bool() const;
    [[nodiscard]] Value get_path(const std::string& key) const;
};

class MiniJsonParser {
private:
    std::string src;
    size_t pos = 0;

    void skip_ws();
    char peek();
    char get();
    std::string parse_string();
    Value parse_number();

public:
    explicit MiniJsonParser(std::string s);
    Value parse_value();
};

#endif // AWK_VALUE_HPP
