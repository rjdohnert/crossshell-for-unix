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

#ifndef BASE_OPTIONS_HPP
#define BASE_OPTIONS_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

enum class BaseAlgorithm { Base64, Base32 };

class BaseOptions {
public:
    static constexpr std::string_view APP_NAME = "base";
    static constexpr std::string_view APP_VERSION = "8.0.0";
    static constexpr size_t BUFFER_SIZE = 65536;

    BaseAlgorithm algorithm{BaseAlgorithm::Base64};
    bool decode{false};
    bool ignoreGarbage{false};
    size_t wrapCols{76}; // 0 disables wrapping
    std::optional<std::string> inputPath;
    std::optional<std::string> outputPath;
    int outputFormat{0};
    std::string pipeCommand;

    static void printHelp();
    static void printVersion();
    static bool parse(int argc, char* argv[], BaseOptions& opts);
};

#endif // BASE_OPTIONS_HPP
