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

#include "bc_app.hpp"
#include "engine.hpp"
#include "options.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

int BcApplication::run(int argc, char** argv) {
    BcOptions options;
    std::vector<std::string> expressions;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--json") options.output_format = 1;
        else if (arg == "--csv") options.output_format = 2;
        else if (arg == "--table") options.output_format = 3;
        else if (arg == "--pipe" && i + 1 < argc) options.pipe_command = argv[++i];
        else expressions.push_back(arg);
    }

    if (!expressions.empty()) {
        std::string arg = expressions.front();
        if (arg == "--help" || arg == "-h") {
            BcOptions::show_help();
            return 0;
        }
        if (arg == "--version") {
            BcOptions::show_version();
            return 0;
        }
        if (arg == "--") {
            if (expressions.size() > 1) {
                std::string expr;
                for (size_t i = 1; i < expressions.size(); ++i) {
                    if (i > 1) expr += ' ';
                    expr += expressions[i];
                }
                return BcEngine::evaluate(expr, options);
            }
            return 0;
        }
        if (expressions.size() == 1) {
            return BcEngine::evaluate(arg, options);
        }
    }

    BcOptions::show_help();
    std::string input;

    while (true) {
        if (options.output_format == 0 && options.pipe_command.empty()) std::cout << "bc> ";
        if (!std::getline(std::cin, input)) {
            break;
        }

        std::string command = input;
        command.erase(command.begin(), std::find_if(command.begin(), command.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        command.erase(std::find_if(command.rbegin(), command.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), command.end());

        if (command.empty()) {
            continue;
        }

        if (command == "exit" || command == "quit") {
            break;
        }

        if (command == "help") {
            BcOptions::show_help();
            continue;
        }

        BcEngine::evaluate(input, options);
    }

    return 0;
}
