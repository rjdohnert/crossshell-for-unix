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

#ifndef DC_BIG_NUMBER_HPP
#define DC_BIG_NUMBER_HPP

#include <vector>
#include <string>

class BigInt {
public:
    std::vector<int> digits; // Little-endian: digits[0] is least significant

    BigInt();
    explicit BigInt(long long v);
    explicit BigInt(const std::string& s);

    void trim();
    bool is_zero() const;

    static int cmp(const BigInt& a, const BigInt& b);
    static BigInt add(const BigInt& a, const BigInt& b);
    static BigInt sub(const BigInt& a, const BigInt& b);
    static BigInt mul(const BigInt& a, const BigInt& b);
    static void divmod(const BigInt& a, const BigInt& b, BigInt& q, BigInt& r);
    static BigInt sqrt(const BigInt& a);

    void shift_left_10(int k);
    void shift_right_10(int k);
    long long to_long_long() const;
};

class BigNumber {
public:
    bool negative = false;
    BigInt mag;
    int scale = 0;

    BigNumber();
    explicit BigNumber(long long v, int s = 0);

    static BigNumber parse(const std::string& str, int ibase = 10);
    std::string to_string(int obase = 10) const;

    int get_scale() const { return scale; }
    int length() const { return static_cast<int>(mag.digits.size()); }
    long long to_integer() const;
};

BigNumber op_add(const BigNumber& a, const BigNumber& b);
BigNumber op_sub(const BigNumber& a, const BigNumber& b);
BigNumber op_mul(const BigNumber& a, const BigNumber& b, int precision);
bool op_div(const BigNumber& a, const BigNumber& b, int precision, BigNumber& quot);

#endif // DC_BIG_NUMBER_HPP
