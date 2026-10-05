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

#include "options.hpp"
#include <iostream>

void BcOptions::show_help() {
    std::cout << R"HELP(bc(1)                  CrossShell for UNIX Reference Manual                   bc(1)

    NAME
        bc - Evaluate mathematical expressions interactively or from scripts.

    SYNOPSIS
        bc [OPTIONS]
        bc [OPTIONS] EXPRESSION
        bc [OPTIONS] -- EXPRESSION...

    DESCRIPTION
        Evaluates double-precision mathematical expressions supplied on the
        command line or through standard input. With no expression, bc starts
        an interactive session. Results can be formatted for scripts or sent
        through another command using a Windows pipe.

    OPTIONS
        --json
            Format each result as a JSON object.

        --csv
            Format each result as CSV with expression and result columns.

        --table
            Format each result as a two-column table.

        --pipe COMMAND
            Send result output through COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Display version information.

    OPERATORS AND FUNCTIONS
        +, -, *, /, %, ^
            Arithmetic operators. The ^ operator performs exponentiation.

        ( expression )
            Group an expression to control evaluation order.

        pi, e
            Built-in mathematical constants.

        sin, cos, tan, asin, acos, atan
            Trigonometric functions. Angles are measured in radians.

        sqrt, log, ln, abs, exp
            Square root, base-10 logarithm, natural logarithm, absolute
            value, and exponential functions.

    INTERACTIVE COMMANDS
        help
            Display this reference manual.

        exit, quit
            End the interactive session.

    EXAMPLES
        bc "2 + 3 * 4"
            Evaluate one command-line expression.

        bc -- 2 + 3 * 4
            Join the remaining arguments and evaluate them as one expression.

        bc --json "sqrt(81)"
            Format the result as JSON.

        bc --table --pipe "more" "2 ^ 16"
            Send a tabular result through another command.

    CrossShell for UNIX                                                        bc(1)
    )HELP";
}

void BcOptions::show_version() {
    std::cout << "bc (IBM AIX 7.3)\n";
}
