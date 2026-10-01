/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
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

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <cstdint>
#include <cstdio>

#define A2PDF_VERSION "1.1.0"

// --- Configuration Options ---
struct Config {
    std::string inputFile;
    std::string outputFile;
    bool outputPostScript = false;
    std::string fontName = "Courier"; // Courier, Helvetica, Times-Roman
    double fontSize = 10.0;
    bool showLineNumbers = false;
    bool landscape = false;
    bool disableWrapping = false;
    std::string title = "";
    double marginMargin = 36.0; // 0.5 inch (36 points)
    int tabSize = 4;
    int outputFormat = 0;
    std::string pipeCommand;
};

class A2PdfOutput {
public:
    static std::string format(const std::string& value, int mode) {
        if (mode == 1) return "{\"message\":\"" + escape(value) + "\"}\n";
        if (mode == 2) return "\"message\"\n\"" + csv(value) + "\"\n";
        return "MESSAGE\n-------\n" + value;
    }
    static void write(const std::string& value, int mode, const std::string& pipe) {
        std::string text = format(value, mode);
        if (!pipe.empty()) { FILE* p = _popen(pipe.c_str(), "w"); if (p) { fwrite(text.data(), 1, text.size(), p); _pclose(p); } }
        else std::cout << text;
    }
private:
    static std::string escape(const std::string& value) { std::string out; for (char c : value) { if (c == '"' || c == '\\') out += '\\'; if (c == '\n') out += "\\n"; else if (c != '\r') out += c; } return out; }
    static std::string csv(const std::string& value) { std::string out; for (char c : value) out += c == '"' ? "\"\"" : std::string(1, c); return out; }
};

// --- UTF-8 & WinAnsi Encoding Engine ---
std::u32string Utf8ToUtf32(const std::string& str) {
    std::u32string result;
    size_t i = 0;
    while (i < str.length()) {
        uint32_t cp = 0;
        unsigned char c = str[i];
        if (c < 0x80) {
            cp = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < str.length()) cp = ((c & 0x1F) << 6) | (str[i + 1] & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 < str.length()) cp = ((c & 0x0F) << 12) | ((str[i + 1] & 0x3F) << 6) | (str[i + 2] & 0x3F);
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 < str.length()) cp = ((c & 0x07) << 18) | ((str[i + 1] & 0x3F) << 12) | ((str[i + 2] & 0x3F) << 6) | (str[i + 3] & 0x3F);
            i += 4;
        } else {
            cp = '?';
            i += 1;
        }
        result.push_back(cp);
    }
    return result;
}

// Map Unicode Code Points to PDF/PS WinAnsiEncoding (Windows-1252) Bytes
std::string Utf32ToWinAnsiPdfString(const std::u32string& str32) {
    std::string output = "";
    for (char32_t cp : str32) {
        unsigned char b = '?';
        if (cp >= 0x20 && cp <= 0x7E) {
            if (cp == '(' || cp == ')' || cp == '\\') {
                output += '\\';
                output += static_cast<char>(cp);
                continue;
            }
            b = static_cast<unsigned char>(cp);
        } else if (cp >= 0xA0 && cp <= 0xFF) {
            b = static_cast<unsigned char>(cp); // Direct Latin-1 mapping
        } else {
            // Windows-1252 Extensions Mapping
            switch (cp) {
                case 0x20AC: b = 0x80; break; // Euro €
                case 0x201A: b = 0x82; break; // Single low quote ‚
                case 0x201E: b = 0x84; break; // Double low quote „
                case 0x2026: b = 0x85; break; // Ellipsis …
                case 0x2018: b = 0x91; break; // Left single quote ‘
                case 0x2019: b = 0x92; break; // Right single quote ’
                case 0x201C: b = 0x93; break; // Left double quote “
                case 0x201D: b = 0x94; break; // Right double quote ”
                case 0x2022: b = 0x95; break; // Bullet •
                case 0x2013: b = 0x96; break; // En dash –
                case 0x2014: b = 0x97; break; // Em dash —
                case 0x2122: b = 0x99; break; // Trademark ™
                default:     b = '?';    break; // Fallback for unsupported glyphs
            }
        }
        output += static_cast<char>(b);
    }
    return output;
}

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

// --- Intelligent Line Wrapping Engine ---
struct ProcessedLine {
    std::string pdfFormattedText;
};

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

void PrintHelp(const char* exeName = "a2pdf") {
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

// --- PostScript Level 3 Generator Engine ---
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

// --- Native PDF 1.4 Generator Engine ---
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

// --- Main Program Entry ---
static int a2pdf_main(int argc, char* argv[]) {
    Config cfg;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            PrintHelp(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "a2pdf version " << A2PDF_VERSION << "\n";
            return 0;
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

class A2PdfApplication {
public:
    int run(int argc, char* argv[]) const { return a2pdf_main(argc, argv); }
};

int main(int argc, char* argv[]) {
    return A2PdfApplication().run(argc, argv);
}