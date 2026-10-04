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

#include "cli.hpp"
#include <iostream>

void PrintHelp(const char* exeName) {
    (void)exeName;
    std::cout << R"(a2pdf(1)                CrossShell for UNIX Reference Manual                   a2pdf(1)

    NAME
        a2pdf - convert text or UTF-8 input into PDF or PostScript output

    SYNOPSIS
        a2pdf [OPTIONS] [INPUT_FILE]

    DESCRIPTION
        Converts plain text or UTF-8 encoded files into PDF 1.4 or PostScript Level 3.
        Includes automatic line wrapping, UTF-8/WinAnsi decoding (accents, currency,
        smart quotes), pagination, line numbering, and header formatting.

    OPTIONS
        -o, --output FILE
            Destination output filename (default: <input>.pdf or .ps).

        -p, --postscript
            Output PostScript (.ps) instead of PDF.

        -f, --font NAME
            Font family: Courier (default), Helvetica, Times-Roman.

        -s, --font-size PT
            Font point size (default: 10.0).

        -l, --line-numbers
            Print 6-digit line numbers on the left margin.

        --landscape
            Render in landscape orientation.

        --no-wrap
            Disable automatic long-line wrapping.

        --title TEXT
            Set document header title.

        --tab-size SPACES
            Set tab expansion size (default: 4).

        --output FORMAT
            Select table, csv, json, or xml output. The default is table.

        --json
            Shortcut for --output json.

        --csv
            Shortcut for --output csv.

        --table
            Shortcut for --output table.

        --pipe COMMAND
            Send formatted status through COMMAND.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version information.

    EXAMPLES
        a2pdf document.txt -o document.pdf
            Convert UTF-8 text file to PDF.

        a2pdf -l -f Courier main.cpp -o main.pdf
            Convert source code with line numbers and line wrapping.

        a2pdf -p --landscape server.log -o server_log.ps
            Output PostScript in landscape mode.

        a2pdf document.txt --json
            Emit conversion metadata and status as JSON.

    CrossShell for UNIX                                                      a2pdf(1)
)";
}

ParseResult ParseCommandLine(int argc, char* argv[], Config& cfg) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            PrintHelp(argv[0]);
            return ParseResult::ExitSuccess;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "a2pdf version " << A2PDF_VERSION << "\n";
            return ParseResult::ExitSuccess;
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            cfg.outputFile = argv[++i];
        } else if (arg == "-p" || arg == "--postscript") {
            cfg.outputPostScript = true;
        } else if ((arg == "-f" || arg == "--font") && i + 1 < argc) {
            cfg.fontName = argv[++i];
        } else if ((arg == "-s" || arg == "--font-size") && i + 1 < argc) {
            cfg.fontSize = std::stod(argv[++i]);
        } else if (arg == "-l" || arg == "--line-numbers") {
            cfg.showLineNumbers = true;
        } else if (arg == "--landscape") {
            cfg.landscape = true;
        } else if (arg == "--no-wrap") {
            cfg.disableWrapping = true;
        } else if (arg == "--title" && i + 1 < argc) {
            cfg.title = argv[++i];
        } else if (arg == "--tab-size" && i + 1 < argc) {
            cfg.tabSize = std::stoi(argv[++i]);
        } else if (arg == "--json") {
            cfg.outputFormat = 1;
        } else if (arg == "--csv") {
            cfg.outputFormat = 2;
        } else if (arg == "--table") {
            cfg.outputFormat = 3;
        } else if (arg == "--pipe" && i + 1 < argc) {
            cfg.pipeCommand = argv[++i];
        } else if (arg[0] != '-') {
            cfg.inputFile = arg;
        }
    }

    // Auto-derive Output Filename
    if (cfg.outputFile.empty()) {
        if (!cfg.inputFile.empty() && cfg.inputFile != "-") {
            size_t dotPos = cfg.inputFile.find_last_of('.');
            std::string baseName = (dotPos == std::string::npos) ? cfg.inputFile : cfg.inputFile.substr(0, dotPos);
            cfg.outputFile = baseName + (cfg.outputPostScript ? ".ps" : ".pdf");
        } else {
            cfg.outputFile = cfg.outputPostScript ? "output.ps" : "output.pdf";
        }
    }

    return ParseResult::Success;
}
