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

#include "parser.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

ExpressionParser::ExpressionParser(std::string str) : src(std::move(str)), pos(0) {}

char ExpressionParser::peek() const {
    if (pos < src.length()) return src[pos];
    return '\0';
}

char ExpressionParser::get() {
    if (pos < src.length()) return src[pos++];
    return '\0';
}

void ExpressionParser::skip_whitespace() {
    while (pos < src.length() && std::isspace(static_cast<unsigned char>(src[pos]))) {
        pos++;
    }
}

double ExpressionParser::parse() {
    pos = 0;
    double res = expression();
    skip_whitespace();
    if (pos < src.length()) {
        throw std::runtime_error("Unexpected character: '" + std::string(1, src[pos]) + "'");
    }
    return res;
}

// Handles addition and subtraction
double ExpressionParser::expression() {
    double left = term();
    while (true) {
        skip_whitespace();
        char op = peek();
        if (op == '+' || op == '-') {
            get();
            double right = term();
            if (op == '+') left += right;
            else left -= right;
        } else {
            break;
        }
    }
    return left;
}

// Handles multiplication, division, and modulo
double ExpressionParser::term() {
    double left = factor();
    while (true) {
        skip_whitespace();
        char op = peek();
        if (op == '*' || op == '/' || op == '%') {
            get();
            double right = factor();
            if (op == '*') {
                left *= right;
            } else if (op == '/') {
                if (right == 0.0) throw std::runtime_error("Division by zero");
                left /= right;
            } else {
                if (right == 0.0) throw std::runtime_error("Modulo by zero");
                left = std::fmod(left, right);
            }
        } else {
            break;
        }
    }
    return left;
}

// Handles exponentiation (right-associative)
double ExpressionParser::factor() {
    double left = primary();
    skip_whitespace();
    if (peek() == '^') {
        get();
        double right = factor();
        left = std::pow(left, right);
    }
    return left;
}

// Handles numbers, parentheses, functions, and unary operators
double ExpressionParser::primary() {
    skip_whitespace();
    char c = peek();
    if (c == '\0') {
        throw std::runtime_error("Unexpected end of expression");
    }

    // Unary plus and minus
    if (c == '+') {
        get();
        return primary();
    }
    if (c == '-') {
        get();
        return -primary();
    }

    // Parentheses
    if (c == '(') {
        get();
        double val = expression();
        skip_whitespace();
        if (get() != ')') {
            throw std::runtime_error("Missing closing parenthesis");
        }
        return val;
    }

    // Numbers
    if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
        return number();
    }

    // Functions and Constants
    if (std::isalpha(static_cast<unsigned char>(c))) {
        std::string id = name();
        skip_whitespace();

        if (peek() == '(') {
            get(); // consume '('
            double arg = expression();
            skip_whitespace();
            if (get() != ')') {
                throw std::runtime_error("Missing closing parenthesis for function '" + id + "'");
            }

            // Normalize function names to lowercase
            std::transform(id.begin(), id.end(), id.begin(), [](unsigned char ch){ return std::tolower(ch); });

            if (id == "sin") return std::sin(arg);
            if (id == "cos") return std::cos(arg);
            if (id == "tan") return std::tan(arg);
            if (id == "asin") return std::asin(arg);
            if (id == "acos") return std::acos(arg);
            if (id == "atan") return std::atan(arg);
            if (id == "sqrt") {
                if (arg < 0) throw std::runtime_error("Square root of negative number");
                return std::sqrt(arg);
            }
            if (id == "log") {
                if (arg <= 0) throw std::runtime_error("Logarithm (base 10) of non-positive number");
                return std::log10(arg);
            }
            if (id == "ln") {
                if (arg <= 0) throw std::runtime_error("Natural logarithm of non-positive number");
                return std::log(arg);
            }
            if (id == "abs") return std::abs(arg);
            if (id == "exp") return std::exp(arg);

            throw std::runtime_error("Unknown function: '" + id + "'");
        } else {
            // Check for constants
            std::transform(id.begin(), id.end(), id.begin(), [](unsigned char ch){ return std::tolower(ch); });
            if (id == "pi") return 3.14159265358979323846;
            if (id == "e") return 2.71828182845904523536;
            throw std::runtime_error("Unknown constant: '" + id + "'");
        }
    }

    throw std::runtime_error("Unexpected character: '" + std::string(1, c) + "'");
}

double ExpressionParser::number() {
    size_t start = pos;
    bool has_dot = false;
    bool has_e = false;

    while (pos < src.length()) {
        char c = src[pos];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            pos++;
        } else if (c == '.' && !has_dot && !has_e) {
            has_dot = true;
            pos++;
        } else if ((c == 'e' || c == 'E') && !has_e) {
            has_e = true;
            pos++;
            if (pos < src.length() && (src[pos] == '+' || src[pos] == '-')) {
                pos++;
            }
        } else {
            break;
        }
    }
    if (start == pos) {
        throw std::runtime_error("Expected a number");
    }
    return std::stod(src.substr(start, pos - start));
}

std::string ExpressionParser::name() {
    size_t start = pos;
    while (pos < src.length() && std::isalpha(static_cast<unsigned char>(src[pos]))) {
        pos++;
    }
    return src.substr(start, pos - start);
}
