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

#include "engine.hpp"
#include "reporter.hpp"
#include <cctype>
#include <iostream>
#include <vector>

std::string BasenameEngine::extract(std::string path, const std::string& suffix) {
    if (path.empty()) {
        return "";
    }

    // 1. Strip trailing path separators
    size_t lastNonSlash = path.find_last_not_of("/\\");
    if (lastNonSlash == std::string::npos) {
        return std::string(1, path[0]);
    }
    path = path.substr(0, lastNonSlash + 1);

    // 2. Extract last component
    size_t lastSlash = path.find_last_of("/\\");
    std::string result = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;

    // 3. Handle Windows drive prefixes (e.g. C:foo -> foo)
    if (result.size() >= 2 && result[1] == ':' && std::isalpha(static_cast<unsigned char>(result[0]))) {
        if (result.size() > 2) {
            result = result.substr(2);
        }
    }

    // 4. Remove suffix
    if (!suffix.empty() && result != suffix) {
        if (result.size() >= suffix.size()) {
            size_t suffixPos = result.size() - suffix.size();
            if (result.compare(suffixPos, suffix.size(), suffix) == 0) {
                result = result.substr(0, suffixPos);
            }
        }
    }

    return result;
}

int BasenameEngine::execute(const BasenameOptions& opts, const char* progName) {
    std::vector<std::string> results;

    if (opts.multiple) {
        if (opts.paths.empty()) {
            BasenameOptions::printUsage(progName);
            return 1;
        }
        for (const auto& str : opts.paths) {
            results.push_back(extract(str, opts.suffix));
        }
    } else {
        if (opts.paths.empty()) {
            BasenameOptions::printUsage(progName);
            return 1;
        } else if (opts.paths.size() == 1) {
            results.push_back(extract(opts.paths[0], ""));
        } else if (opts.paths.size() == 2) {
            results.push_back(extract(opts.paths[0], opts.paths[1]));
        } else {
            std::cerr << "basename: extra operand '" << opts.paths[2] << "'\n";
            BasenameOptions::printUsage(progName);
            return 1;
        }
    }

    return BasenameReporter::report(results, opts.outputFormat, opts.zeroTerminated, opts.pipeCommand);
}
