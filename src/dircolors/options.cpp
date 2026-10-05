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

#include "options.hpp"
#include <iostream>
#include <cstdlib>

namespace dircolors {

void DircolorsOptions::printHelp(const char* /*progName*/) {
    std::cout << R"(dircolors(1)               CrossShell for UNIX Reference Manual               dircolors(1)

    NAME
        dircolors - output commands to set the LS_COLORS environment variable

    SYNOPSIS
        dircolors [OPTION]... [FILE]

    DESCRIPTION
        Output commands to set the LS_COLORS environment variable.

        If FILE is specified, read it to determine what colors to use for
        which file types and extensions. Otherwise, a precompiled database is used.

    OPTIONS
        -b, --sh, --bourne-shell
            Output Bourne shell code to set LS_COLORS.
        -c, --csh, --c-shell
            Output C shell code to set LS_COLORS.
        -p, --powershell, --ps
            Output PowerShell code to set $env:LS_COLORS.
        -cmd, --cmd
            Output Windows CMD batch code to set LS_COLORS.
        -p, --print-database
            Output default configuration database.
        --json, --csv, --table
            Output in structured format.
        --pipe COMMAND
            Send output through COMMAND.
        -h, --help
            Display this help and exit.
        --version
            Output version information and exit.

    EXAMPLES
        dircolors -b
            Output Bourne shell commands to configure LS_COLORS.

        dircolors -p
            Output PowerShell commands to configure $env:LS_COLORS.

        dircolors --print-database
            Print the internal database of default color bindings.

    CrossShell for UNIX                                                    dircolors(1)
)";
}

void DircolorsOptions::printVersion() {
    std::cout << "dircolors 1.0\n";
}

bool DircolorsOptions::parse(int argc, char* argv[], DircolorsOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h" || arg == "/?") {
            printHelp(argv[0]);
            std::exit(0);
        } else if (arg == "--version") {
            printVersion();
            std::exit(0);
        } else if (arg == "-b" || arg == "--sh" || arg == "--bourne-shell") {
            opts.shell = TargetShell::Bourne;
        } else if (arg == "-c" || arg == "--csh" || arg == "--c-shell") {
            opts.shell = TargetShell::CShell;
        } else if (arg == "--powershell" || arg == "--ps") {
            opts.shell = TargetShell::PowerShell;
        } else if (arg == "-cmd" || arg == "--cmd") {
            opts.shell = TargetShell::Cmd;
        } else if (arg == "-p" || arg == "--print-database") {
            opts.shell = TargetShell::PrintDatabase;
        } else if (arg == "--json") {
            opts.outputFormat = 1;
        } else if (arg == "--csv") {
            opts.outputFormat = 2;
        } else if (arg == "--table") {
            opts.outputFormat = 3;
        } else if (arg == "--pipe" && i + 1 < argc) {
            opts.pipeCommand = argv[++i];
        } else if (arg[0] == '-' && arg.size() > 1) {
            std::cerr << "dircolors: unrecognized option '" << arg << "'\n";
            return false;
        } else {
            opts.configFile = arg;
        }
    }
    return true;
}

} // namespace dircolors
