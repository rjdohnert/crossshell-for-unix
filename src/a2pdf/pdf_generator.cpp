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

#include "pdf_generator.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>

bool GeneratePdf(const Config& cfg, const std::vector<ProcessedLine>& lines) {
    std::ofstream out(cfg.outputFile, std::ios::binary);
    if (!out.is_open()) return false;

    double pageWidth = cfg.landscape ? 792.0 : 612.0;
    double pageHeight = cfg.landscape ? 612.0 : 792.0;
    double lineHeight = cfg.fontSize * 1.2;
    double topMargin = pageHeight - cfg.marginMargin - 30.0;
    double bottomMargin = cfg.marginMargin;
    int linesPerPage = static_cast<int>((topMargin - bottomMargin) / lineHeight);

    std::string documentTitle = cfg.title.empty() ? cfg.inputFile : cfg.title;

    int pageCount = static_cast<int>((lines.size() + linesPerPage - 1) / linesPerPage);
    if (pageCount == 0) pageCount = 1;

    std::vector<size_t> offsets;
    offsets.push_back(0); // Dummy for Obj 0

    auto StartObject = [&](int objId) {
        offsets.push_back(static_cast<size_t>(out.tellp()));
        out << objId << " 0 obj\n";
    };

    // Header
    out << "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";

    // 1. Catalog
    StartObject(1);
    out << "<</Type /Catalog /Pages 2 0 R>>\nendobj\n";

    // Calculate Page Object IDs
    std::vector<int> pageObjIds;
    int currentObjId = 4;
    for (int i = 0; i < pageCount; ++i) {
        pageObjIds.push_back(currentObjId);
        currentObjId += 2;
    }

    // 2. Pages Root
    StartObject(2);
    out << "<</Type /Pages /Count " << pageCount << " /Kids [";
    for (int id : pageObjIds) out << id << " 0 R ";
    out << "]>>\nendobj\n";

    // 3. Base Font Object (WinAnsiEncoding)
    StartObject(3);
    out << "<</Type /Font /Subtype /Type1 /BaseFont /" << cfg.fontName << " /Encoding /WinAnsiEncoding>>\nendobj\n";

    // Generate Pages & Content Streams
    size_t lineIdx = 0;
    for (int p = 0; p < pageCount; ++p) {
        int pageObjId = pageObjIds[p];
        int contentsObjId = pageObjId + 1;

        std::ostringstream streamBody;

        // Draw Header Line
        streamBody << "0.5 w\n";
        streamBody << cfg.marginMargin << " " << (pageHeight - cfg.marginMargin - 8) << " m "
                   << (pageWidth - cfg.marginMargin) << " " << (pageHeight - cfg.marginMargin - 8) << " l S\n";

        // Draw Header Text
        streamBody << "BT /F1 9 Tf " << cfg.marginMargin << " " << (pageHeight - cfg.marginMargin) << " Td ("
                   << documentTitle << ") Tj ET\n";
        streamBody << "BT /F1 9 Tf " << (pageWidth - cfg.marginMargin - 50) << " " << (pageHeight - cfg.marginMargin) << " Td (Page "
                   << (p + 1) << ") Tj ET\n";

        // Draw Body Text
        streamBody << "BT /F1 " << cfg.fontSize << " Tf " << lineHeight << " TL\n";
        streamBody << cfg.marginMargin << " " << topMargin << " Td\n";

        for (int l = 0; l < linesPerPage && lineIdx < lines.size(); ++l, ++lineIdx) {
            streamBody << "(" << lines[lineIdx].pdfFormattedText << ") ' ";
        }
        streamBody << "ET\n";

        std::string streamStr = streamBody.str();

        // Write Page Object
        StartObject(pageObjId);
        out << "<</Type /Page /Parent 2 0 R /MediaBox [0 0 " << pageWidth << " " << pageHeight
            << "] /Resources <</Font <</F1 3 0 R>>>> /Contents " << contentsObjId << " 0 R>>\nendobj\n";

        // Write Content Stream Object
        StartObject(contentsObjId);
        out << "<</Length " << streamStr.length() << ">>\nstream\n" << streamStr << "endstream\nendobj\n";
    }

    // XREF Table
    size_t xrefOffset = static_cast<size_t>(out.tellp());
    out << "xref\n0 " << offsets.size() << "\n0000000000 65535 f \n";
    for (size_t i = 1; i < offsets.size(); ++i) {
        out << std::setw(10) << std::setfill('0') << offsets[i] << " 00000 n \n";
    }

    // Trailer
    out << "trailer\n<</Size " << offsets.size() << " /Root 1 0 R>>\n";
    out << "startxref\n" << xrefOffset << "\n%%EOF\n";

    return true;
}
