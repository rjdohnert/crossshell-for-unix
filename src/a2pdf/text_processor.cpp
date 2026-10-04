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

#include "text_processor.hpp"
#include "encoding.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

// --- Tab Expansion ---
std::u32string ExpandTabsU32(const std::u32string& input, int tabSize) {
    std::u32string output;
    for (char32_t c : input) {
        if (c == '\t') {
            size_t spacesNeeded = tabSize - (output.length() % tabSize);
            output.append(spacesNeeded, U' ');
        } else if (c != '\r') {
            output.push_back(c);
        }
    }
    return output;
}

// --- Text File Ingestion ---
std::vector<std::string> ReadRawLines(const std::string& filename) {
    std::vector<std::string> lines;
    std::istream* input = &std::cin;
    std::ifstream fileStream;

    if (!filename.empty() && filename != "-") {
        fileStream.open(filename, std::ios::binary);
        if (!fileStream.is_open()) {
            std::cerr << "[a2pdf] Error: Cannot open input file: " << filename << "\n";
            return lines;
        }
        input = &fileStream;
    }

    std::string line;
    while (std::getline(*input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

std::vector<ProcessedLine> WrapAndProcessLines(const std::vector<std::string>& rawLines, const Config& cfg) {
    std::vector<ProcessedLine> result;

    double pageWidth = cfg.landscape ? 792.0 : 612.0;
    double printableWidth = pageWidth - (2.0 * cfg.marginMargin);
    double approxCharWidth = cfg.fontSize * 0.6; // Font metric ratio for Monospaced/Courier

    int prefixLength = cfg.showLineNumbers ? 8 : 0; // "  1234  "
    int maxCols = static_cast<int>(printableWidth / approxCharWidth);
    int textCols = (std::max)(10, maxCols - prefixLength);

    for (size_t i = 0; i < rawLines.size(); ++i) {
        std::u32string u32line = ExpandTabsU32(Utf8ToUtf32(rawLines[i]), cfg.tabSize);

        std::string lineNumStr = "";
        if (cfg.showLineNumbers) {
            std::ostringstream ss;
            ss << std::setw(6) << (i + 1) << "  ";
            lineNumStr = ss.str();
        }

        if (cfg.disableWrapping || u32line.length() <= static_cast<size_t>(textCols)) {
            std::string fullLine = lineNumStr + Utf32ToWinAnsiPdfString(u32line);
            result.push_back({ fullLine });
        } else {
            // Line Wrapping Execution
            size_t start = 0;
            bool firstChunk = true;
            while (start < u32line.length()) {
                std::u32string chunk = u32line.substr(start, textCols);
                std::string formattedChunk = "";

                if (firstChunk) {
                    formattedChunk = lineNumStr + Utf32ToWinAnsiPdfString(chunk);
                    firstChunk = false;
                } else {
                    std::string indent = cfg.showLineNumbers ? "        \\ " : "  \\ ";
                    formattedChunk = indent + Utf32ToWinAnsiPdfString(chunk);
                }

                result.push_back({ formattedChunk });
                start += textCols;
            }
        }
    }

    if (result.empty()) {
        result.push_back({ "" });
    }

    return result;
}
