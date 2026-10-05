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
#include "codec.hpp"
#include "reporter.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

BaseEngine::BaseEngine(BaseOptions opts) : options(std::move(opts)) {}

int BaseEngine::execute() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    std::istream* in = &std::cin;
    std::ifstream fileIn;

    if (options.inputPath && *options.inputPath != "-") {
        fileIn.open(*options.inputPath, std::ios::binary);
        if (!fileIn.is_open()) {
            std::cerr << "base: cannot open '" << *options.inputPath << "'\n";
            return 1;
        }
        in = &fileIn;
    }

    std::ostream* out = &std::cout;
    std::ofstream fileOut;

    if (options.outputPath) {
        fileOut.open(*options.outputPath, std::ios::binary);
        if (!fileOut.is_open()) {
            std::cerr << "base: cannot open '" << *options.outputPath << "' for writing\n";
            return 1;
        }
        out = &fileOut;
    }

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(*in)), std::istreambuf_iterator<char>());

    if (options.decode) {
        try {
            std::string inputStr(buffer.begin(), buffer.end());
            std::vector<uint8_t> decoded = (options.algorithm == BaseAlgorithm::Base64)
                ? Base64Codec::decode(inputStr, options.ignoreGarbage)
                : Base32Codec::decode(inputStr, options.ignoreGarbage);

            std::string text(decoded.begin(), decoded.end());
            BaseReporter::dispatch(text, options.outputFormat, options.pipeCommand, *out);
        } catch (const std::exception& e) {
            std::cerr << "base: error decoding input: " << e.what() << "\n";
            return 1;
        }
    } else {
        std::string encoded = (options.algorithm == BaseAlgorithm::Base64)
            ? Base64Codec::encode(buffer)
            : Base32Codec::encode(buffer);

        if (options.wrapCols > 0) {
            std::string wrapped;
            for (size_t i = 0; i < encoded.size(); i += options.wrapCols) {
                wrapped += encoded.substr(i, options.wrapCols) + "\n";
            }
            encoded = wrapped;
        } else {
            encoded += "\n";
        }

        BaseReporter::dispatch(encoded, options.outputFormat, options.pipeCommand, *out);
    }

    return 0;
}
