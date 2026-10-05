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
#include <iostream>
#include <utility>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace detab {

DetabEngine::DetabEngine(DetabOptions opts) : options(std::move(opts)) {}

size_t DetabEngine::calculateSpaces(size_t col) const {
    if (options.tabStops.empty()) {
        size_t w = options.singleTabWidth;
        if (w == 0) return 1;
        return w - (col % w);
    } else {
        for (size_t stop : options.tabStops) {
            if (stop > col + 1) {
                return (stop - 1) - col;
            }
        }
        return 1;
    }
}

bool DetabEngine::processStream(FILE* fp, std::string* capture) const {
    size_t col = 0;
    bool inInitialBlanks = true;

    setvbuf(fp, NULL, _IOFBF, IO_BUFFER_SIZE);

    auto writeChar = [capture](int value) {
        return capture ? (capture->push_back(static_cast<char>(value)), value) : fputc(value, stdout);
    };

    int ch = 0;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '\n') {
            if (writeChar(ch) == EOF) return false;
            col = 0;
            inInitialBlanks = true;
        } else if (ch == '\r') {
            if (writeChar(ch) == EOF) return false;
            col = 0;
        } else if (ch == '\b') {
            if (writeChar(ch) == EOF) return false;
            if (col > 0) col--;
        } else if (ch == '\t') {
            if (!options.initialOnly || inInitialBlanks) {
                size_t numSpaces = calculateSpaces(col);
                for (size_t i = 0; i < numSpaces; ++i) {
                    if (writeChar(' ') == EOF) return false;
                }
                col += numSpaces;
            } else {
                if (writeChar('\t') == EOF) return false;
                col += calculateSpaces(col);
            }
        } else {
            if (writeChar(ch) == EOF) return false;
            col++;
            if (ch != ' ') {
                inInitialBlanks = false;
            }
        }
    }
    return true;
}

int DetabEngine::execute(const char* progName) {
    if (options.showHelp) {
        DetabOptions::printHelp(progName);
        return 0;
    }

    if (options.showVersion) {
        DetabOptions::printVersion();
        return 0;
    }

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    setvbuf(stdout, NULL, _IOFBF, IO_BUFFER_SIZE);

    if (options.files.empty()) {
        options.files.push_back("-");
    }

    bool success = true;
    std::string captured;
    bool needCapture = (options.outputFormat != OutputFormat::Default || !options.pipeCommand.empty());
    std::string* capturePtr = needCapture ? &captured : nullptr;

    for (const std::string& filename : options.files) {
        FILE* fp = nullptr;

        if (filename == "-") {
            fp = stdin;
        } else {
#ifdef _WIN32
            errno_t err = fopen_s(&fp, filename.c_str(), "rb");
            if (err != 0 || !fp) {
                std::cerr << "expand: " << filename << ": No such file or directory\n";
                success = false;
                continue;
            }
#else
            fp = fopen(filename.c_str(), "rb");
            if (!fp) {
                std::cerr << "expand: " << filename << ": No such file or directory\n";
                success = false;
                continue;
            }
#endif
        }

        if (!processStream(fp, capturePtr)) {
            if (fp != stdin) fclose(fp);
            return 0;
        }

        if (fp != stdin) {
            fclose(fp);
        }
    }

    if (needCapture) {
        DetabReporter::dispatch(captured, options.outputFormat, options.pipeCommand);
    }
    fflush(stdout);
    return success ? 0 : 1;
}

} // namespace detab
