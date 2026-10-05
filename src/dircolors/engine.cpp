/*
 * Copyright (c) 2025, R. J. Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "engine.hpp"
#include "database_parser.hpp"
#include "reporter.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <utility>

namespace dircolors {

DircolorsEngine::DircolorsEngine(DircolorsOptions opts) : options(std::move(opts)) {}

int DircolorsEngine::execute() {
    if (options.shell == TargetShell::PrintDatabase) {
        std::cout << ColorDatabaseParser::getDefaultDatabase();
        return 0;
    }

    std::string lsColors;
    if (!options.configFile.empty()) {
        std::ifstream file(options.configFile);
        if (!file.is_open()) {
            std::cerr << "dircolors: cannot open '" << options.configFile << "'\n";
            return 1;
        }
        lsColors = ColorDatabaseParser::parseDatabase(file);
    } else {
        std::istringstream iss(ColorDatabaseParser::getDefaultDatabase());
        lsColors = ColorDatabaseParser::parseDatabase(iss);
    }

    std::string command;
    switch (options.shell) {
        case TargetShell::Bourne:
            command = "LS_COLORS='" + lsColors + "'; export LS_COLORS\n";
            break;
        case TargetShell::CShell:
            command = "setenv LS_COLORS '" + lsColors + "'\n";
            break;
        case TargetShell::PowerShell:
            command = "$env:LS_COLORS = \"" + lsColors + "\"\n";
            break;
        case TargetShell::Cmd:
            command = "SET LS_COLORS=" + lsColors + "\n";
            break;
        default:
            command = lsColors + "\n";
            break;
    }

    return DircolorsReporter::dispatch(command, options.outputFormat, options.pipeCommand);
}

} // namespace dircolors
