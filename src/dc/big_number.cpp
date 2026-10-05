/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
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

#include "big_number.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>

BigInt::BigInt() : digits({0}) {}

BigInt::BigInt(long long v) {
    v = std::abs(v);
    if (v == 0) { digits = {0}; return; }
    while (v > 0) {
        digits.push_back(static_cast<int>(v % 10));
        v /= 10;
    }
}

BigInt::BigInt(const std::string& s) {
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        if (std::isdigit(static_cast<unsigned char>(*it))) {
            digits.push_back(*it - '0');
        }
    }
    trim();
}

void BigInt::trim() {
    while (digits.size() > 1 && digits.back() == 0) {
        digits.pop_back();
    }
    if (digits.empty()) digits = {0};
}

bool BigInt::is_zero() const {
    return digits.size() == 1 && digits[0] == 0;
}

int BigInt::cmp(const BigInt& a, const BigInt& b) {
    if (a.digits.size() != b.digits.size())
        return a.digits.size() < b.digits.size() ? -1 : 1;
    for (int i = static_cast<int>(a.digits.size()) - 1; i >= 0; --i) {
        if (a.digits[i] != b.digits[i])
            return a.digits[i] < b.digits[i] ? -1 : 1;
    }
    return 0;
}

BigInt BigInt::add(const BigInt& a, const BigInt& b) {
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

BigInt BigInt::sub(const BigInt& a, const BigInt& b) { // assumes a >= b
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

BigInt BigInt::mul(const BigInt& a, const BigInt& b) {
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

void BigInt::divmod(const BigInt& a, const BigInt& b, BigInt& q, BigInt& r) {
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

void BigInt::shift_left_10(int k) {
    if (is_zero() || k <= 0) return;
    digits.insert(digits.begin(), k, 0);
}

void BigInt::shift_right_10(int k) {
    if (k <= 0) return;
    if (static_cast<size_t>(k) >= digits.size()) {
        digits = {0};
    } else {
        digits.erase(digits.begin(), digits.begin() + k);
        trim();
    }
}

long long BigInt::to_long_long() const {
    long long val = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
        val = val * 10 + digits[i];
    }
    return val;
}

BigInt BigInt::sqrt(const BigInt& a) {
    if (a.is_zero()) return BigInt(0);
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

BigNumber::BigNumber() : negative(false), mag(0), scale(0) {}
BigNumber::BigNumber(long long v, int s) : negative(v < 0), mag(std::abs(v)), scale(s) {}

BigNumber BigNumber::parse(const std::string& str, int ibase) {
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

std::string BigNumber::to_string(int obase) const {
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

long long BigNumber::to_integer() const {
    BigInt copy = mag;
    copy.shift_right_10(scale);
    long long val = copy.to_long_long();
    return negative ? -val : val;
}

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
