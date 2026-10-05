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
 */

#include "calculator.hpp"
#include <iostream>
#include <cctype>
#include <algorithm>

void Register::store(std::shared_ptr<IDCValue> val) {
    if (stack.empty()) stack.push_back(val);
    else stack.back() = val;
}

std::shared_ptr<IDCValue> Register::load() const {
    return stack.empty() ? nullptr : stack.back();
}

void Register::push(std::shared_ptr<IDCValue> val) {
    stack.push_back(val);
}

std::shared_ptr<IDCValue> Register::pop() {
    if (stack.empty()) return nullptr;
    auto val = stack.back();
    stack.pop_back();
    return val;
}

void DeskCalculator::push(std::shared_ptr<IDCValue> val) {
    stack.push_back(val);
}

std::shared_ptr<IDCValue> DeskCalculator::pop() {
    if (stack.empty()) {
        std::cerr << "dc: stack empty\n";
        return nullptr;
    }
    auto v = stack.back();
    stack.pop_back();
    return v;
}

std::shared_ptr<IDCValue> DeskCalculator::peek() const {
    if (stack.empty()) {
        std::cerr << "dc: stack empty\n";
        return nullptr;
    }
    return stack.back();
}

Register& DeskCalculator::get_register(const std::string& name) {
    return registers[name];
}

void DeskCalculator::execute(const std::string& input) {
    size_t i = 0;
    size_t n = input.length();

    while (i < n) {
        if (quit_levels > 0) return;

        char ch = input[i];

        if (std::isspace(static_cast<unsigned char>(ch))) {
            ++i;
            continue;
        }
        if (ch == '#') {
            while (i < n && input[i] != '\n') ++i;
            continue;
        }

        // String literal: [ ... ]
        if (ch == '[') {
            int depth = 1;
            size_t start = ++i;
            while (i < n && depth > 0) {
                if (input[i] == '[') depth++;
                else if (input[i] == ']') depth--;
                i++;
            }
            push(std::make_shared<DCString>(input.substr(start, i - start - 1)));
            continue;
        }

        // Numeric literal
        if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '.' || ch == '_' ||
            (ibase > 10 && std::isupper(static_cast<unsigned char>(ch)))) {
            size_t start = i;
            if (ch == '_') ++i;
            bool has_dot = (ch == '.');
            while (i < n) {
                char c = input[i];
                if (std::isdigit(static_cast<unsigned char>(c)) ||
                    (ibase > 10 && std::isupper(static_cast<unsigned char>(c)))) {
                    ++i;
                } else if (c == '.' && !has_dot) {
                    has_dot = true;
                    ++i;
                } else {
                    break;
                }
            }
            std::string num_str = input.substr(start, i - start);
            push(std::make_shared<DCNumber>(BigNumber::parse(num_str, ibase)));
            continue;
        }

        // Operators
        ++i;
        switch (ch) {
            case '+': {
                auto b = pop(); auto a = pop();
                if (a && b && !a->is_string() && !b->is_string()) {
                    auto na = std::static_pointer_cast<DCNumber>(a);
                    auto nb = std::static_pointer_cast<DCNumber>(b);
                    push(std::make_shared<DCNumber>(op_add(na->num, nb->num)));
                }
                break;
            }
            case '-': {
                auto b = pop(); auto a = pop();
                if (a && b && !a->is_string() && !b->is_string()) {
                    auto na = std::static_pointer_cast<DCNumber>(a);
                    auto nb = std::static_pointer_cast<DCNumber>(b);
                    push(std::make_shared<DCNumber>(op_sub(na->num, nb->num)));
                }
                break;
            }
            case '*': {
                auto b = pop(); auto a = pop();
                if (a && b && !a->is_string() && !b->is_string()) {
                    auto na = std::static_pointer_cast<DCNumber>(a);
                    auto nb = std::static_pointer_cast<DCNumber>(b);
                    push(std::make_shared<DCNumber>(op_mul(na->num, nb->num, precision)));
                }
                break;
            }
            case '/': {
                auto b = pop(); auto a = pop();
                if (a && b && !a->is_string() && !b->is_string()) {
                    auto na = std::static_pointer_cast<DCNumber>(a);
                    auto nb = std::static_pointer_cast<DCNumber>(b);
                    BigNumber quot;
                    if (op_div(na->num, nb->num, precision, quot)) {
                        push(std::make_shared<DCNumber>(quot));
                    } else {
                        std::cerr << "dc: divide by zero\n";
                        push(a); push(b);
                    }
                }
                break;
            }
            case '%': {
                auto b = pop(); auto a = pop();
                if (a && b && !a->is_string() && !b->is_string()) {
                    auto na = std::static_pointer_cast<DCNumber>(a);
                    auto nb = std::static_pointer_cast<DCNumber>(b);
                    BigNumber quot;
                    if (op_div(na->num, nb->num, precision, quot)) {
                        BigNumber prod = op_mul(quot, nb->num, precision);
                        push(std::make_shared<DCNumber>(op_sub(na->num, prod)));
                    } else {
                        std::cerr << "dc: remainder by zero\n";
                        push(a); push(b);
                    }
                }
                break;
            }
            case '~': {
                auto b = pop(); auto a = pop();
                if (a && b && !a->is_string() && !b->is_string()) {
                    auto na = std::static_pointer_cast<DCNumber>(a);
                    auto nb = std::static_pointer_cast<DCNumber>(b);
                    BigNumber quot;
                    if (op_div(na->num, nb->num, precision, quot)) {
                        BigNumber prod = op_mul(quot, nb->num, precision);
                        push(std::make_shared<DCNumber>(quot));
                        push(std::make_shared<DCNumber>(op_sub(na->num, prod)));
                    } else {
                        std::cerr << "dc: divide by zero\n";
                        push(a); push(b);
                    }
                }
                break;
            }
            case '^': {
                auto b = pop(); auto a = pop();
                if (a && b && !a->is_string() && !b->is_string()) {
                    auto na = std::static_pointer_cast<DCNumber>(a);
                    auto nb = std::static_pointer_cast<DCNumber>(b);
                    long long expo = nb->num.to_integer();
                    BigNumber base = na->num;
                    BigNumber res(1, 0);
                    long long e = std::abs(expo);
                    while (e > 0) {
                        if (e % 2 == 1) res = op_mul(res, base, precision);
                        base = op_mul(base, base, precision);
                        e /= 2;
                    }
                    if (expo < 0) {
                        BigNumber inv;
                        op_div(BigNumber(1), res, precision, inv);
                        res = inv;
                    }
                    push(std::make_shared<DCNumber>(res));
                }
                break;
            }
            case 'v': {
                auto a = pop();
                if (a && !a->is_string()) {
                    auto na = std::static_pointer_cast<DCNumber>(a);
                    if (na->num.negative) {
                        std::cerr << "dc: square root of negative number\n";
                    } else {
                        int target_scale = std::max(na->num.scale, precision);
                        BigInt shifted = na->num.mag;
                        shifted.shift_left_10(2 * target_scale - na->num.scale);
                        BigNumber root;
                        root.mag = BigInt::sqrt(shifted);
                        root.scale = target_scale;
                        push(std::make_shared<DCNumber>(root));
                    }
                }
                break;
            }
            case 'c': stack.clear(); break;
            case 'd': {
                auto val = peek();
                if (val) push(val);
                break;
            }
            case 'r': {
                if (stack.size() >= 2) {
                    std::swap(stack[stack.size() - 1], stack[stack.size() - 2]);
                } else {
                    std::cerr << "dc: stack empty\n";
                }
                break;
            }
            case 'R': {
                auto val = pop();
                if (val && !val->is_string()) {
                    long long k = std::static_pointer_cast<DCNumber>(val)->num.to_integer();
                    if (k > 0 && stack.size() >= static_cast<size_t>(k)) {
                        auto it = stack.end() - k;
                        auto item = *it;
                        stack.erase(it);
                        stack.push_back(item);
                    }
                }
                break;
            }
            case 'z': {
                push(std::make_shared<DCNumber>(BigNumber(static_cast<long long>(stack.size()))));
                break;
            }
            case 'p': {
                auto val = peek();
                if (val) std::cout << val->to_string(obase) << "\n";
                break;
            }
            case 'n': {
                auto val = pop();
                if (val) {
                    std::cout << val->to_string(obase);
                    std::cout.flush();
                }
                break;
            }
            case 'P': {
                auto val = pop();
                if (val) {
                    if (val->is_string()) {
                        std::cout << val->to_string(obase);
                    } else {
                        long long b = std::abs(std::static_pointer_cast<DCNumber>(val)->num.to_integer());
                        std::cout << static_cast<char>(b % 256);
                    }
                    std::cout.flush();
                }
                break;
            }
            case 'f': {
                for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
                    std::cout << (*it)->to_string(obase) << "\n";
                }
                break;
            }
            case 's':
            case 'l':
            case 'S':
            case 'L': {
                if (i < n) {
                    std::string reg(1, input[i++]);
                    auto& r = get_register(reg);
                    if (ch == 's') {
                        auto val = pop();
                        if (val) r.store(val);
                    } else if (ch == 'l') {
                        auto val = r.load();
                        if (val) push(val);
                        else std::cerr << "dc: register '" << reg << "' empty\n";
                    } else if (ch == 'S') {
                        auto val = pop();
                        if (val) r.push(val);
                    } else if (ch == 'L') {
                        auto val = r.pop();
                        if (val) push(val);
                        else std::cerr << "dc: register stack '" << reg << "' empty\n";
                    }
                }
                break;
            }
            case 'k': {
                auto val = pop();
                if (val && !val->is_string()) {
                    precision = std::max(0, static_cast<int>(std::static_pointer_cast<DCNumber>(val)->num.to_integer()));
                }
                break;
            }
            case 'K': push(std::make_shared<DCNumber>(BigNumber(precision))); break;
            case 'i': {
                auto val = pop();
                if (val && !val->is_string()) {
                    long long base = std::static_pointer_cast<DCNumber>(val)->num.to_integer();
                    if (base >= 2 && base <= 36) ibase = static_cast<int>(base);
                    else std::cerr << "dc: input base must be between 2 and 36\n";
                }
                break;
            }
            case 'I': push(std::make_shared<DCNumber>(BigNumber(ibase))); break;
            case 'o': {
                auto val = pop();
                if (val && !val->is_string()) {
                    long long base = std::static_pointer_cast<DCNumber>(val)->num.to_integer();
                    if (base >= 2) obase = static_cast<int>(base);
                    else std::cerr << "dc: output base must be at least 2\n";
                }
                break;
            }
            case 'O': push(std::make_shared<DCNumber>(BigNumber(obase))); break;
            case 'Z': {
                auto val = pop();
                if (val) push(std::make_shared<DCNumber>(BigNumber(val->get_length())));
                break;
            }
            case 'X': {
                auto val = pop();
                if (val) push(std::make_shared<DCNumber>(BigNumber(val->get_scale())));
                break;
            }
            case 'x': {
                auto val = pop();
                if (val) {
                    if (val->is_string()) {
                        execute(std::static_pointer_cast<DCString>(val)->str);
                    } else {
                        push(val);
                    }
                }
                break;
            }
            case '<':
            case '>':
            case '=':
            case '!': {
                bool invert = false;
                char op = ch;
                if (ch == '!') {
                    if (i < n && (input[i] == '<' || input[i] == '>' || input[i] == '=')) {
                        op = input[i++];
                        invert = true;
                    }
                }
                if (i < n) {
                    std::string reg(1, input[i++]);
                    auto b = pop(); auto a = pop();
                    if (a && b && !a->is_string() && !b->is_string()) {
                        auto na = std::static_pointer_cast<DCNumber>(a);
                        auto nb = std::static_pointer_cast<DCNumber>(b);
                        BigNumber diff = op_sub(na->num, nb->num);
                        bool cond = false;
                        if (op == '<') cond = diff.negative;
                        else if (op == '>') cond = (!diff.negative && !diff.mag.is_zero());
                        else if (op == '=') cond = diff.mag.is_zero();
                        if (invert) cond = !cond;

                        if (cond) {
                            auto macro = get_register(reg).load();
                            if (macro && macro->is_string()) {
                                execute(std::static_pointer_cast<DCString>(macro)->str);
                            }
                        }
                    }
                }
                break;
            }
            case 'q': quit_levels = 1; return;
            case 'Q': {
                auto val = pop();
                if (val && !val->is_string()) {
                    quit_levels = static_cast<int>(std::static_pointer_cast<DCNumber>(val)->num.to_integer());
                    return;
                }
                break;
            }
            case '?': {
                std::string line;
                if (std::getline(std::cin, line)) execute(line);
                break;
            }
            default:
                break;
        }
    }
}
