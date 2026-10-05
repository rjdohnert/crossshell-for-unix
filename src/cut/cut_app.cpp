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

#include "cut_app.hpp"
#include "stream_processor.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>

int CutApplication::Run(int argc, char* argv[]) {
    CutOptions opts;
    bool exitEarly = false;
    if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
        OptionParser::DisplayHelp();
        return 1;
    }
    if (exitEarly) {
        return 0;
    }

    std::ostringstream captured;
    std::streambuf* oldOutput = nullptr;
    if (opts.output_format || !opts.pipe_command.empty()) {
        oldOutput = std::cout.rdbuf(captured.rdbuf());
    }

    bool success = true;
    if (opts.files.empty()) {
        StreamProcessor::ProcessStream(std::cin, opts, std::cout);
    } else {
        for (const auto& file : opts.files) {
            if (file == "-") {
                StreamProcessor::ProcessStream(std::cin, opts, std::cout);
            } else {
                std::ifstream ifs(file);
                if (!ifs.is_open()) {
                    std::cerr << "cut: " << file << ": No such file or directory\n";
                    success = false;
                    continue;
                }
                StreamProcessor::ProcessStream(ifs, opts, std::cout);
            }
        }
    }

    if (oldOutput) {
        std::cout.rdbuf(oldOutput);
        std::string data = captured.str();
        std::string text = (opts.output_format == 1) ? "{\"output\":\"" + data + "\"}\n"
                         : (opts.output_format == 2) ? "\"output\"\n\"" + data + "\"\n"
                         : "OUTPUT\n------\n" + data;
        if (!opts.pipe_command.empty()) {
#ifdef _WIN32
            FILE* pipe = _popen(opts.pipe_command.c_str(), "w");
            if (!pipe) return 1;
            fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
#else
            FILE* pipe = popen(opts.pipe_command.c_str(), "w");
            if (!pipe) return 1;
            fwrite(text.data(), 1, text.size(), pipe);
            pclose(pipe);
#endif
        } else {
            std::cout << text;
        }
    }

    return success ? 0 : 1;
}
