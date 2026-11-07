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

#include "gsar_app.hpp"
#include "gsar_cli.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

int main(int argc, char* argv[]) {
    Options options;
    std::string error;
    if (!CommandLineParser::parse(argc, argv, options, error)) {
        std::cerr << "[gsar] Error: " << error << '\n'
                  << "Use -h or --help for usage information.\n";
        return 1;
    }

    if (options.showHelp) {
        const std::string executableName = argc > 0 && argv[0] != nullptr
            ? std::filesystem::path(argv[0]).stem().string()
            : "gsar";
        CommandLineParser::printHelp(executableName);
        return 0;
    }
    if (options.showVersion) {
        std::cout << "gsar version 1.0.0 (CrossShell for UNIX)\n";
        return 0;
    }

    try {
        return GSARApp(std::move(options)).run();
    } catch (const std::exception& exception) {
        std::cerr << "[gsar] Fatal error: " << exception.what() << '\n';
        return 1;
    }
}
