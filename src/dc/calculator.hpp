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

#ifndef DC_CALCULATOR_HPP
#define DC_CALCULATOR_HPP

#include "values.hpp"
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>

class Register {
public:
    std::vector<std::shared_ptr<IDCValue>> stack;

    void store(std::shared_ptr<IDCValue> val);
    std::shared_ptr<IDCValue> load() const;
    void push(std::shared_ptr<IDCValue> val);
    std::shared_ptr<IDCValue> pop();
};

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

    void push(std::shared_ptr<IDCValue> val);
    std::shared_ptr<IDCValue> pop();
    std::shared_ptr<IDCValue> peek() const;
    Register& get_register(const std::string& name);
    void execute(const std::string& input);
};

#endif // DC_CALCULATOR_HPP
