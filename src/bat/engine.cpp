/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, cmd-extended contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
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

#include "engine.hpp"
#include "color.hpp"
#include "highlighter.hpp"
#include "reporter.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

BatEngine::BatEngine(BatOptions opts) : options(std::move(opts)) {}

void BatEngine::processStream(std::istream& in, const std::string& label, std::ostream& out) const {
    const BatLanguage language = SyntaxHighlighter::detectLanguage(label, options.forcedLanguage);
    if (options.showHeader && !options.plain) {
        out << BatColor::GRAY << "───────┬─────────────────────────────────────────\n"
            << "       │ File: " << label << "\n"
            << "───────┼─────────────────────────────────────────\n" << BatColor::RESET;
    }

    HighlightState state;
    std::string line;
    int lineNum = 1;

    while (std::getline(in, line)) {
        if (options.showLineNumbers && !options.plain) {
            out << BatColor::GRAY << std::setw(6) << lineNum++ << " │ " << BatColor::RESET;
        } else if (options.showLineNumbers) {
            out << std::setw(6) << lineNum++ << "  ";
        }
        out << SyntaxHighlighter::highlightLine(line, state, language, options.plain) << "\n";
    }

    if (options.showHeader && !options.plain) {
        out << BatColor::GRAY << "───────┴─────────────────────────────────────────\n" << BatColor::RESET;
    }
}

int BatEngine::execute() {
    SyntaxHighlighter::initConsole();

    std::ostringstream captured;
    std::ostream* outStream = (options.outputFormat || !options.pipeCommand.empty()) ? &captured : &std::cout;

    for (const auto& file : options.files) {
        if (file == "-") {
            processStream(std::cin, "standard input", *outStream);
        } else {
            std::ifstream in(file);
            if (!in.is_open()) {
                std::cerr << "bat: cannot open '" << file << "'\n";
                continue;
            }
            processStream(in, file, *outStream);
        }
    }

    if (options.outputFormat || !options.pipeCommand.empty()) {
        BatReporter::dispatch(captured.str(), options.outputFormat, options.pipeCommand);
    }

    return 0;
}
