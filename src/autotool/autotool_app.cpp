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

#include "autotool_app.hpp"
#include "types.hpp"
#include "help.hpp"
#include "automake.hpp"
#include "autoconf.hpp"
#include "build_engine.hpp"
#include "port_engine.hpp"
#include "libtool.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

using std::string;
using std::vector;
using std::cerr;

int AutotoolApp::run(int argc, char* argv[]) {
    if (argc < 2) {
        print_main_help();
        return EXIT_SUCCESS;
    }

    string subcommand = argv[1];
    vector<string> sub_args;
    for (int i = 2; i < argc; ++i) sub_args.push_back(argv[i]);

    if (subcommand == "-h" || subcommand == "--help") {
        print_main_help();
    } else if (subcommand == "-V" || subcommand == "--version") {
        std::cout << TOOL_NAME << " " << VERSION << "\n";
    } else if (subcommand == "help") {
        if (sub_args.empty()) print_main_help();
        else if (sub_args[0] == "automake") print_automake_help();
        else if (sub_args[0] == "autoconf") print_autoconf_help();
        else if (sub_args[0] == "libtool") print_libtool_help();
        else if (sub_args[0] == "build") print_build_help();
        else if (sub_args[0] == "port") print_port_help();
        else print_main_help();
    } else if (subcommand == "automake") {
        AutomakeModule::run_automake(sub_args);
    } else if (subcommand == "autoconf") {
        AutoconfModule::run_autoconf(sub_args);
    } else if (subcommand == "libtool") {
        return LibtoolModule::run_libtool(sub_args);
    } else if (subcommand == "build") {
        BuildModule::run_native_build(sub_args);
    } else if (subcommand == "port") {
        PortModule::run_gnu_port(sub_args);
    } else {
        cerr << TOOL_NAME << ": error: unknown command '" << subcommand << "'\n";
        print_main_help();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
