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
#include <cstdlib>
#include <iostream>

void BaseOptions::printHelp() {
    std::cout << R"(base(1)                 CrossShell for UNIX Reference Manual                  base(1)

    NAME
        base - encode and decode Base64/Base32 data streams

    SYNOPSIS
        base [OPTIONS] [FILE]

    DESCRIPTION
        base encodes or decodes FILE, or standard input, to standard output.
        It supports both Base64 and Base32 transformations and can also emit
        structured output formats for pipelines and scripting use.

    OPTIONS
        --base64
            Use Base64 encoding/decoding (default).

        --base32
            Use Base32 encoding/decoding.

        -d, --decode
            Decode data instead of encoding it.

        -i, --ignore-garbage
            When decoding, ignore non-alphabet characters.

        -w, --wrap COLS
            Wrap encoded lines after COLS characters (default 76, 0 disables
            wrapping).

        -o, --output FILE
            Write output to FILE instead of standard output.

        --json, --csv
            Output structured records.

        --pipe COMMAND
            Send output through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        base --base64 sample.txt
            Encode file using Base64.

        base -d --output decoded.bin encoded.txt
            Decode Base64 file into binary output.

        base --json --pipe jq
            Output structured JSON and pipe to jq.

    CrossShell for UNIX                                                    base(1)
)";
}

void BaseOptions::printVersion() {
    std::cout << APP_NAME << " " << APP_VERSION << "\n";
}

bool BaseOptions::parse(int argc, char* argv[], BaseOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h" || arg == "/?") {
            printHelp();
            std::exit(0);
        } else if (arg == "--version" || arg == "-V") {
            printVersion();
            std::exit(0);
        } else if (arg == "--base64") {
            opts.algorithm = BaseAlgorithm::Base64;
        } else if (arg == "--base32") {
            opts.algorithm = BaseAlgorithm::Base32;
        } else if (arg == "-d" || arg == "--decode") {
            opts.decode = true;
        } else if (arg == "-i" || arg == "--ignore-garbage") {
            opts.ignoreGarbage = true;
        } else if (arg == "--json") {
            opts.outputFormat = 1;
        } else if (arg == "--csv") {
            opts.outputFormat = 2;
        } else if (arg == "--table") {
            opts.outputFormat = 3;
        } else if (arg == "--pipe" && i + 1 < argc) {
            opts.pipeCommand = argv[++i];
        } else if (arg.rfind("-w", 0) == 0) {
            opts.wrapCols = std::stoul(arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "76"));
        } else if (arg.rfind("--wrap=", 0) == 0) {
            opts.wrapCols = std::stoul(arg.substr(7));
        } else if (arg.rfind("-o", 0) == 0) {
            opts.outputPath = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "");
        } else if (arg.rfind("--output=", 0) == 0) {
            opts.outputPath = arg.substr(9);
        } else if (arg[0] == '-' && arg.size() > 1) {
            std::cerr << "base: unrecognized option '" << arg << "'\n";
            return false;
        } else {
            opts.inputPath = arg;
        }
    }
    return true;
}
