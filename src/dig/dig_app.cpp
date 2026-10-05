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

#include "dig_app.hpp"
#include "options.hpp"
#include "query_engine.hpp"
#include "reporter.hpp"
#include <iostream>

namespace dig {

int DigApplication::Run(int argc, wchar_t* argv[]) {
    DigOptions opts;
    if (!opts.Parse(argc, argv)) {
        return 1;
    }

    if (opts.showHelp) {
        opts.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"dig");
        return 0;
    }

    if (opts.showVersion) {
        opts.PrintVersion();
        return 0;
    }

    WinsockScope winsock;
    if (!winsock.IsInitialized()) {
        std::wcerr << L"dig: winsock initialization failed\n";
        return 1;
    }

    DnsQueryEngine engine;
    std::vector<DnsAnswerItem> answers;
    DWORD status = 0;

    if (!engine.Query(opts, answers, status)) {
        std::wcerr << L"dig: query failed for " << opts.name << L" (" << status << L")\n";
        return 1;
    }

    DigReporter::EmitOutput(opts, answers);
    return answers.empty() ? 1 : 0;
}

} // namespace dig
