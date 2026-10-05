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

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cctype>

// ============================================================================
// Arbitrary Precision Decimal Engine
// ============================================================================

class BigInt {
public:
    std::vector<int> digits; // Little-endian: digits[0] is least significant

    BigInt() : digits({0}) {}
    BigInt(long long v) {
        v = std::abs(v);
        if (v == 0) { digits = {0}; return; }
        while (v > 0) {
            digits.push_back(static_cast<int>(v % 10));
            v /= 10;
        }
    }
    BigInt(const std::string& s) {
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            if (std::isdigit(static_cast<unsigned char>(*it))) {
                digits.push_back(*it - '0');
            }
        }
        trim();
    }

    void trim() {
        while (digits.size() > 1 && digits.back() == 0) {
            digits.pop_back();
        }
        if (digits.empty()) digits = {0};
    }

    bool is_zero() const {
        return digits.size() == 1 && digits[0] == 0;
    }

    static int cmp(const BigInt& a, const BigInt& b) {
        if (a.digits.size() != b.digits.size())
            return a.digits.size() < b.digits.size() ? -1 : 1;
        for (int i = static_cast<int>(a.digits.size()) - 1; i >= 0; --i) {
            if (a.digits[i] != b.digits[i])
                return a.digits[i] < b.digits[i] ? -1 : 1;
        }
        return 0;
    }

    static BigInt add(const BigInt& a, const BigInt& b) {
        BigInt res;
        res.digits.clear();
        int carry = 0;
        size_t n = std::max(a.digits.size(), b.digits.size());
        for (size_t i = 0; i < n || carry; ++i) {
            int sum = carry;
            if (i < a.digits.size()) sum += a.digits[i];
            if (i < b.digits.size()) sum += b.digits[i];
            res.digits.push_back(sum % 10);
            carry = sum / 10;
        }
        res.trim();
        return res;
    }

    static BigInt sub(const BigInt& a, const BigInt& b) { // assumes a >= b
        BigInt res;
        res.digits.clear();
        int borrow = 0;
        for (size_t i = 0; i < a.digits.size(); ++i) {
            int diff = a.digits[i] - borrow - (i < b.digits.size() ? b.digits[i] : 0);
            if (diff < 0) {
                diff += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }
            res.digits.push_back(diff);
        }
        res.trim();
        return res;
    }

    static BigInt mul(const BigInt& a, const BigInt& b) {
        if (a.is_zero() || b.is_zero()) return BigInt(0);
        BigInt res;
        res.digits.assign(a.digits.size() + b.digits.size(), 0);
        for (size_t i = 0; i < a.digits.size(); ++i) {
            int carry = 0;
            for (size_t j = 0; j < b.digits.size() || carry; ++j) {
                long long cur = res.digits[i + j] + carry +
                    1LL * a.digits[i] * (j < b.digits.size() ? b.digits[j] : 0);
                res.digits[i + j] = static_cast<int>(cur % 10);
                carry = static_cast<int>(cur / 10);
            }
        }
        res.trim();
        return res;
    }

    static void divmod(const BigInt& a, const BigInt& b, BigInt& q, BigInt& r) {
        q.digits.clear();
        r.digits.clear();
        BigInt cur;
        cur.digits.clear();

        for (int i = static_cast<int>(a.digits.size()) - 1; i >= 0; --i) {
            if (!(cur.is_zero() && a.digits[i] == 0)) {
                cur.digits.insert(cur.digits.begin(), a.digits[i]);
            }
            cur.trim();

            int low = 0, high = 9, d = 0;
            while (low <= high) {
                int mid = (low + high) / 2;
                BigInt prod = mul(b, BigInt(mid));
                if (cmp(prod, cur) <= 0) {
                    d = mid;
                    low = mid + 1;
                } else {
                    high = mid - 1;
                }
            }
            q.digits.push_back(d);
            cur = sub(cur, mul(b, BigInt(d)));
        }
        std::reverse(q.digits.begin(), q.digits.end());
        q.trim();
        r = cur;
        r.trim();
    }

    void shift_left_10(int k) {
        if (is_zero() || k <= 0) return;
        digits.insert(digits.begin(), k, 0);
    }

    void shift_right_10(int k) {
        if (k <= 0) return;
        if (static_cast<size_t>(k) >= digits.size()) {
            digits = {0};
        } else {
            digits.erase(digits.begin(), digits.begin() + k);
            trim();
        }
    }

    long long to_long_long() const {
        long long val = 0;
        for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
            val = val * 10 + digits[i];
        }
        return val;
    }

    static BigInt sqrt(const BigInt& a) {
        if (a.is_zero()) return BigInt(0);
        // Digit-by-digit root extraction
        BigInt res(0), rem(0);
        std::vector<int> d = a.digits;
        if (d.size() % 2 != 0) d.push_back(0);

        for (int i = static_cast<int>(d.size()) - 1; i >= 0; i -= 2) {
            rem.shift_left_10(2);
            rem = add(rem, BigInt(d[i] * 10 + d[i - 1]));

            int x = 0;
            BigInt current_res_20 = mul(res, BigInt(20));
            for (int candidate = 1; candidate <= 9; ++candidate) {
                BigInt test = mul(add(current_res_20, BigInt(candidate)), BigInt(candidate));
                if (cmp(test, rem) <= 0) {
                    x = candidate;
                } else {
                    break;
                }
            }
            rem = sub(rem, mul(add(current_res_20, BigInt(x)), BigInt(x)));
            res.shift_left_10(1);
            res = add(res, BigInt(x));
        }
        return res;
    }
};

class BigNumber {
public:
    bool negative = false;
    BigInt mag;
    int scale = 0;

    BigNumber() : negative(false), mag(0), scale(0) {}
    BigNumber(long long v, int s = 0) : negative(v < 0), mag(std::abs(v)), scale(s) {}

    static BigNumber parse(const std::string& str, int ibase = 10) {
        BigNumber result;
        std::string s = str;
        if (!s.empty() && s[0] == '_') {
            result.negative = true;
            s = s.substr(1);
        }

        if (ibase == 10) {
            size_t dot_pos = s.find('.');
            if (dot_pos == std::string::npos) {
                result.mag = BigInt(s);
                result.scale = 0;
            } else {
                std::string int_part = s.substr(0, dot_pos);
                std::string frac_part = s.substr(dot_pos + 1);
                result.mag = BigInt(int_part + frac_part);
                result.scale = static_cast<int>(frac_part.length());
            }
        } else {
            // General ibase conversion (bases 2 to 36)
            const std::string charset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            size_t dot_pos = s.find('.');
            std::string int_part = (dot_pos == std::string::npos) ? s : s.substr(0, dot_pos);
            std::string frac_part = (dot_pos == std::string::npos) ? "" : s.substr(dot_pos + 1);

            BigInt val(0);
            BigInt base_val(ibase);
            for (char ch : int_part) {
                char upper_ch = static_cast<char>(std::toupper(ch));
                size_t d = charset.find(upper_ch);
                if (d != std::string::npos && d < static_cast<size_t>(ibase)) {
                    val = BigInt::add(BigInt::mul(val, base_val), BigInt(d));
                }
            }

            int scale_val = static_cast<int>(frac_part.length());
            for (char ch : frac_part) {
                char upper_ch = static_cast<char>(std::toupper(ch));
                size_t d = charset.find(upper_ch);
                if (d != std::string::npos && d < static_cast<size_t>(ibase)) {
                    val = BigInt::add(BigInt::mul(val, base_val), BigInt(d));
                }
            }
            result.mag = val;
            result.scale = scale_val;
        }

        if (result.mag.is_zero()) result.negative = false;
        return result;
    }

    std::string to_string(int obase = 10) const {
        if (mag.is_zero()) return "0";
        std::string sign = negative ? "_" : "";

        if (obase == 10) {
            std::string s;
            for (int d : mag.digits) s.push_back(static_cast<char>('0' + d));
            std::reverse(s.begin(), s.end());

            if (scale == 0) return sign + s;
            if (static_cast<int>(s.length()) <= scale) {
                s.insert(0, scale - s.length() + 1, '0');
            }
            s.insert(s.length() - scale, ".");
            return sign + s;
        }

        // Custom output base
        const std::string charset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        BigInt int_part, frac_part;
        BigInt divisor(1);
        divisor.shift_left_10(scale);
        BigInt::divmod(mag, divisor, int_part, frac_part);

        std::string int_out;
        BigInt b_obase(obase);
        BigInt q = int_part, r;

        if (q.is_zero()) {
            int_out = "0";
        } else {
            std::vector<std::string> chunks;
            while (!q.is_zero()) {
                BigInt next_q;
                BigInt::divmod(q, b_obase, next_q, r);
                long long rem = r.to_long_long();
                if (obase <= 16) {
                    chunks.push_back(std::string(1, charset[rem]));
                } else {
                    chunks.push_back(std::to_string(rem));
                }
                q = next_q;
            }
            std::reverse(chunks.begin(), chunks.end());
            for (size_t i = 0; i < chunks.size(); ++i) {
                if (obase > 16 && i > 0) int_out += " ";
                int_out += chunks[i];
            }
        }

        if (scale > 0) {
            int_out += ".";
            BigInt cur_frac = frac_part;
            for (int i = 0; i < scale; ++i) {
                cur_frac = BigInt::mul(cur_frac, b_obase);
                BigInt digit_part;
                BigInt::divmod(cur_frac, divisor, digit_part, cur_frac);
                long long d = digit_part.to_long_long();
                if (obase <= 16) {
                    int_out += charset[d];
                } else {
                    int_out += " " + std::to_string(d);
                }
            }
        }
        return sign + int_out;
    }

    int get_scale() const { return scale; }
    int length() const { return static_cast<int>(mag.digits.size()); }
    long long to_integer() const {
        BigInt copy = mag;
        copy.shift_right_10(scale);
        long long val = copy.to_long_long();
        return negative ? -val : val;
    }
};

// ============================================================================
// Operations & Arithmetic Semantics
// ============================================================================

BigNumber op_add(const BigNumber& a, const BigNumber& b) {
    int max_scale = std::max(a.scale, b.scale);
    BigInt ma = a.mag, mb = b.mag;
    ma.shift_left_10(max_scale - a.scale);
    mb.shift_left_10(max_scale - b.scale);

    BigNumber r;
    r.scale = max_scale;
    if (a.negative == b.negative) {
        r.mag = BigInt::add(ma, mb);
        r.negative = a.negative;
    } else {
        int c = BigInt::cmp(ma, mb);
        if (c >= 0) {
            r.mag = BigInt::sub(ma, mb);
            r.negative = a.negative;
        } else {
            r.mag = BigInt::sub(mb, ma);
            r.negative = b.negative;
        }
    }
    if (r.mag.is_zero()) r.negative = false;
    return r;
}

BigNumber op_sub(const BigNumber& a, const BigNumber& b) {
    BigNumber neg_b = b;
    neg_b.negative = !b.negative;
    return op_add(a, neg_b);
}

BigNumber op_mul(const BigNumber& a, const BigNumber& b, int precision) {
    BigNumber r;
    r.mag = BigInt::mul(a.mag, b.mag);
    r.negative = (a.negative != b.negative);
    int total_scale = a.scale + b.scale;
    int target_scale = std::min(total_scale, std::max({a.scale, b.scale, precision}));
    if (total_scale > target_scale) {
        r.mag.shift_right_10(total_scale - target_scale);
    }
    r.scale = target_scale;
    if (r.mag.is_zero()) r.negative = false;
    return r;
}

bool op_div(const BigNumber& a, const BigNumber& b, int precision, BigNumber& quot) {
    if (b.mag.is_zero()) return false;
    BigInt ma = a.mag;
    int shift = precision + b.scale - a.scale;
    if (shift >= 0) {
        ma.shift_left_10(shift);
    }
    BigInt rem;
    BigInt::divmod(ma, b.mag, quot.mag, rem);
    quot.scale = precision;
    quot.negative = (a.negative != b.negative);
    if (quot.mag.is_zero()) quot.negative = false;
    return true;
}

// ============================================================================
// Object-Oriented Polymorphic Value System
// ============================================================================

class IDCValue {
public:
    virtual ~IDCValue() = default;
    virtual std::string to_string(int obase) const = 0;
    virtual bool is_string() const = 0;
    virtual int get_length() const = 0;
    virtual int get_scale() const = 0;
};

class DCNumber : public IDCValue {
public:
    BigNumber num;

    explicit DCNumber(const BigNumber& n) : num(n) {}
    std::string to_string(int obase) const override { return num.to_string(obase); }
    bool is_string() const override { return false; }
    int get_length() const override { return num.length(); }
    int get_scale() const override { return num.get_scale(); }
};

class DCString : public IDCValue {
public:
    std::string str;

    explicit DCString(std::string s) : str(std::move(s)) {}
    std::string to_string(int /*obase*/) const override { return str; }
    bool is_string() const override { return true; }
    int get_length() const override { return static_cast<int>(str.length()); }
    int get_scale() const override { return 0; }
};

// ============================================================================
// Register Stack Architecture
// ============================================================================

class Register {
public:
    std::vector<std::shared_ptr<IDCValue>> stack;

    void store(std::shared_ptr<IDCValue> val) {
        if (stack.empty()) stack.push_back(val);
        else stack.back() = val;
    }

    std::shared_ptr<IDCValue> load() const {
        return stack.empty() ? nullptr : stack.back();
    }

    void push(std::shared_ptr<IDCValue> val) {
        stack.push_back(val);
    }

    std::shared_ptr<IDCValue> pop() {
        if (stack.empty()) return nullptr;
        auto val = stack.back();
        stack.pop_back();
        return val;
    }
};

// ============================================================================
// Core Calculator State Machine
// ============================================================================

class DeskCalculator {
private:
    std::vector<std::shared_ptr<IDCValue>> stack;
    std::unordered_map<std::string, Register> registers;
    int precision = 0;
    int ibase = 10;
    int obase = 10;
    int quit_levels = 0;

public:
    DeskCalculator() = default;

    void push(std::shared_ptr<IDCValue> val) {
        stack.push_back(val);
    }

    std::shared_ptr<IDCValue> pop() {
        if (stack.empty()) {
            std::cerr << "dc: stack empty\n";
            return nullptr;
        }
        auto v = stack.back();
        stack.pop_back();
        return v;
    }

    std::shared_ptr<IDCValue> peek() const {
        if (stack.empty()) {
            std::cerr << "dc: stack empty\n";
            return nullptr;
        }
        return stack.back();
    }

    Register& get_register(const std::string& name) {
        return registers[name];
    }

    void execute(const std::string& input) {
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
};

// ============================================================================
// Comprehensive Help & BSD Documentation
// ============================================================================

const char* const USAGE_MANUAL =
R"(NAME
     dc -- desk calculator (reverse-Polish arbitrary-precision calculator)

SYNOPSIS
     dc [-hV] [-e expression] [-f file] [file ...]

DESCRIPTION
     dc is a reverse-Polish desk calculator that supports arbitrary-precision
     arithmetic. It allows macros, named registers, stack manipulation,
     and variable input and output radixes.

OPTIONS
     -e expression, --expression=expression
             Evaluate expression before reading standard input or files.
     -f file, --file=file
             Read and evaluate commands from file.
     -h, --help
             Display this comprehensive help manual and exit.
     -V, --version
             Display version information and exit.

SYNTAX & COMMAND SUMMARY
     Numbers
         Consist of digits 0-9 and upper-case letters A-Z (for radixes > 10).
         Negative numbers begin with an underscore '_'.
         A decimal point '.' introduces the fractional part.

     Arithmetic
         +      Pop two values, add them, push the result.
         -      Pop two values, subtract the top from the second, push result.
         *      Pop two values, multiply them, push the result.
         /      Pop two values, divide second by top, push the result.
         %      Pop two values, compute remainder of division, push result.
         ~      Pop two values, push quotient, then push remainder.
         ^      Pop two values, compute exponentiation, push result.
         v      Pop top value, compute square root, push result.

     Stack Operations
         c      Clear the evaluation stack.
         d      Duplicate the top value on the stack.
         r      Reverse (swap) the top two values.
         R      Pop n, rotate the top n values on the stack.
         z      Push the current stack depth.

     Registers & Macros
         s<r>   Pop the top of the stack and store it in register <r>.
         l<r>   Load the value from register <r> onto the stack.
         S<r>   Pop the top of the stack and push it onto register stack <r>.
         L<r>   Pop from register stack <r> and push onto the main stack.
         [...]  Enclose macro or string literal.
         x      Pop the top value; if string, execute it as a dc program.

     Conditionals
         <r, >r, =r
                Pop two values; if condition holds (second rel top),
                execute the macro stored in register <r>.
         !<r, !>r, !=r
                Negated conditional operators.

     Radix & Scale
         k      Pop scale precision and set calculation scale.
         K      Push current calculation scale.
         i      Pop input radix and set it (2 through 36).
         I      Push current input radix.
         o      Pop output radix and set it (2 or greater).
         O      Push current output radix.

     Display
         p      Print top value with newline (stack unchanged).
         n      Print top value without newline and pop it.
         P      Print top value as raw byte or string.
         f      Print entire evaluation stack from top to bottom.
)";

// ============================================================================
// CLI & Windows Entry Point
// ============================================================================

class DCApp {
private:
    DeskCalculator calc;

public:
    int run(int argc, char* argv[]) {
        std::vector<std::string> args(argv + 1, argv + argc);
        bool ran_command = false;

        for (size_t i = 0; i < args.size(); ++i) {
            const std::string& arg = args[i];
            if (arg == "-h" || arg == "--help") {
                std::cout << USAGE_MANUAL;
                return 0;
            }
            if (arg == "-V" || arg == "--version") {
                std::cout << "dc 2.0\n";
                return 0;
            }
        }

        for (size_t i = 0; i < args.size(); ++i) {
            const std::string& arg = args[i];
            if ((arg == "-e" || arg == "--expression") && i + 1 < args.size()) {
                calc.execute(args[++i]);
                ran_command = true;
            } else if ((arg == "-f" || arg == "--file") && i + 1 < args.size()) {
                run_file(args[++i]);
                ran_command = true;
            } else if (!arg.empty() && arg[0] != '-') {
                run_file(arg);
                ran_command = true;
            }
        }

        if (!ran_command) {
            std::string line;
            while (std::getline(std::cin, line)) {
                calc.execute(line);
            }
        }
        return 0;
    }

private:
    void run_file(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "dc: cannot open " << filename << "\n";
            return;
        }
        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        calc.execute(content);
    }
};

int main(int argc, char* argv[]) {
    DCApp app;
    return app.run(argc, argv);
}