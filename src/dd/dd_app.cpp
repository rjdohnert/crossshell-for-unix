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

#include "dd_app.hpp"
#include "block_copier.hpp"
#include <iostream>
#include <cstdio>

int DdApplication::Run(int argc, char* argv[]) {
    DdOptions opt;
    bool exitEarly = false;
    if (!m_parser.Parse(argc, argv, opt, exitEarly)) {
        OptionParser::DisplayHelp(argv[0]);
        return 1;
    }
    if (exitEarly) {
        return 0;
    }

    TransferStats stats;
    if (!BlockCopier::Execute(opt, stats)) {
        return 1;
    }

    if (opt.output_format || !opt.pipe_command.empty()) {
        std::string text = (opt.output_format == 1)
            ? "{\"status\":\"success\",\"bytes\":" + std::to_string(stats.total_bytes) + "}\n"
            : (opt.output_format == 2)
            ? "status,bytes\nsuccess," + std::to_string(stats.total_bytes) + "\n"
            : "STATUS\tBYTES\nsuccess\t" + std::to_string(stats.total_bytes) + "\n";
        if (!opt.pipe_command.empty()) {
#ifdef _WIN32
            FILE* pipe = _popen(opt.pipe_command.c_str(), "w");
            if (!pipe) return 1;
            fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
#else
            FILE* pipe = popen(opt.pipe_command.c_str(), "w");
            if (!pipe) return 1;
            fwrite(text.data(), 1, text.size(), pipe);
            pclose(pipe);
#endif
        } else {
            std::cout << text;
        }
    }

    return 0;
}
