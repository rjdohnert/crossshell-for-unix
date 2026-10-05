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

#include "dc_app.hpp"
#include <iostream>
#include <fstream>
#include <vector>

static const char* const USAGE_MANUAL =
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

int DCApp::run(int argc, char* argv[]) {
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

void DCApp::run_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "dc: cannot open " << filename << "\n";
        return;
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    calc.execute(content);
}
