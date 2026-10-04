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

#include "a2pdf_app.hpp"
#include "config.hpp"
#include "cli.hpp"
#include "text_processor.hpp"
#include "pdf_generator.hpp"
#include "ps_generator.hpp"
#include "output_reporter.hpp"
#include <iostream>

int A2PdfApplication::run(int argc, char* argv[]) const {
    Config cfg;
    ParseResult res = ParseCommandLine(argc, argv, cfg);
    if (res == ParseResult::ExitSuccess) return 0;
    if (res == ParseResult::ExitFailure) return 1;

    auto rawLines = ReadRawLines(cfg.inputFile);
    auto processedLines = WrapAndProcessLines(rawLines, cfg);

    bool success = false;
    if (cfg.outputPostScript) {
        success = GeneratePostScript(cfg, processedLines);
    } else {
        success = GeneratePdf(cfg, processedLines);
    }

    if (success) {
        std::string message = "[a2pdf] Successfully generated: " + cfg.outputFile + " (" + (cfg.outputPostScript ? "PostScript Level 3" : "PDF 1.4") + ")";
        if (cfg.outputFormat) A2PdfOutput::write(message, cfg.outputFormat, cfg.pipeCommand);
        else if (!cfg.pipeCommand.empty()) A2PdfOutput::write(message, 3, cfg.pipeCommand);
        else std::cout << message << "\n";
    } else {
        std::cerr << "[a2pdf] Error: Failed to generate output file.\n";
        return 1;
    }

    return 0;
}
