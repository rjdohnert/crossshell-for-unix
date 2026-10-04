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

#include "ps_generator.hpp"
#include <fstream>

bool GeneratePostScript(const Config& cfg, const std::vector<ProcessedLine>& lines) {
    std::ofstream out(cfg.outputFile, std::ios::binary);
    if (!out.is_open()) return false;

    double pageWidth = cfg.landscape ? 792.0 : 612.0;
    double pageHeight = cfg.landscape ? 612.0 : 792.0;
    double lineHeight = cfg.fontSize * 1.2;
    double topMargin = pageHeight - cfg.marginMargin - 30.0;
    double bottomMargin = cfg.marginMargin;
    int linesPerPage = static_cast<int>((topMargin - bottomMargin) / lineHeight);

    std::string documentTitle = cfg.title.empty() ? cfg.inputFile : cfg.title;

    out << "%!PS-Adobe-3.0\n";
    out << "%%Title: " << documentTitle << "\n";
    out << "%%Creator: a2pdf v" << A2PDF_VERSION << "\n";
    out << "%%Orientation: " << (cfg.landscape ? "Landscape" : "Portrait") << "\n";
    out << "%%Pages: (atend)\n";
    out << "%%EndComments\n\n";

    out << "/" << cfg.fontName << " findfont " << cfg.fontSize << " scalefont setfont\n\n";

    size_t lineIdx = 0;
    int pageCount = 0;

    while (lineIdx < lines.size()) {
        pageCount++;
        out << "%%Page: " << pageCount << " " << pageCount << "\n";

        // Header Line
        out << "/" << cfg.fontName << "-Bold findfont 9 scalefont setfont\n";
        out << cfg.marginMargin << " " << (pageHeight - cfg.marginMargin) << " moveto\n";
        out << "(" << documentTitle << ") show\n";
        out << (pageWidth - cfg.marginMargin - 60) << " " << (pageHeight - cfg.marginMargin) << " moveto\n";
        out << "(Page " << pageCount << ") show\n";
        out << "0.5 setlinewidth\n";
        out << cfg.marginMargin << " " << (pageHeight - cfg.marginMargin - 8) << " moveto\n";
        out << (pageWidth - cfg.marginMargin) << " " << (pageHeight - cfg.marginMargin - 8) << " lineto stroke\n";

        // Body Text
        out << "/" << cfg.fontName << " findfont " << cfg.fontSize << " scalefont setfont\n";

        double currentY = topMargin;
        for (int i = 0; i < linesPerPage && lineIdx < lines.size(); ++i, ++lineIdx) {
            out << cfg.marginMargin << " " << currentY << " moveto\n";
            out << "(" << lines[lineIdx].pdfFormattedText << ") show\n";
            currentY -= lineHeight;
        }

        out << "showpage\n\n";
    }

    out << "%%Trailer\n";
    out << "%%Pages: " << pageCount << "\n";
    out << "%%EOF\n";

    return true;
}
