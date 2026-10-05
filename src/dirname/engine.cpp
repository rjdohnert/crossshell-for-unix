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
#include "reporter.hpp"
#include <cctype>

namespace dirname_util {

bool DirnameEngine::isSlash(char c) {
    return c == '/' || c == '\\';
}

std::string DirnameEngine::extract(const std::string& path) {
    if (path.empty()) {
        return ".";
    }

    std::string prefix;
    std::string rest = path;

    // 1. Handle UNC Network Paths: \\server\share\...
    if (path.length() >= 2 && isSlash(path[0]) && isSlash(path[1]) && 
        (path.length() == 2 || !isSlash(path[2]))) {
        size_t serverEnd = path.find_first_of("/\\", 2);
        if (serverEnd != std::string::npos) {
            size_t shareEnd = path.find_first_of("/\\", serverEnd + 1);
            if (shareEnd != std::string::npos) {
                prefix = path.substr(0, shareEnd);
                rest = path.substr(shareEnd);
            } else {
                return path;
            }
        } else {
            return path;
        }
    }
    // 2. Handle Drive Letters: C:\... or C:foo
    else if (path.length() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':') {
        prefix = path.substr(0, 2);
        rest = path.substr(2);
    }

    if (rest.empty()) {
        return prefix.empty() ? "." : prefix;
    }

    bool allSlashes = true;
    for (char c : rest) {
        if (!isSlash(c)) {
            allSlashes = false;
            break;
        }
    }

    if (allSlashes) {
        if (!prefix.empty()) {
            return prefix + "\\";
        } else {
            return std::string(1, rest[0]);
        }
    }

    size_t end = rest.length();
    while (end > 0 && isSlash(rest[end - 1])) {
        --end;
    }
    rest = rest.substr(0, end);

    size_t lastSlash = rest.find_last_of("/\\");

    if (lastSlash == std::string::npos) {
        if (!prefix.empty()) {
            return prefix;
        } else {
            return ".";
        }
    }

    size_t dirEnd = lastSlash;
    while (dirEnd > 0 && isSlash(rest[dirEnd - 1])) {
        --dirEnd;
    }

    if (dirEnd == 0) {
        if (!prefix.empty()) {
            return prefix + "\\";
        } else {
            return std::string(1, rest[lastSlash]);
        }
    }

    return prefix + rest.substr(0, dirEnd);
}

int DirnameEngine::execute(const DirnameOptions& opts) {
    std::vector<std::string> results;
    for (const auto& path : opts.paths) {
        results.push_back(extract(path));
    }
    return DirnameReporter::report(results, opts.outputFormat, opts.zeroTerminated, opts.pipeCommand);
}

} // namespace dirname_util
