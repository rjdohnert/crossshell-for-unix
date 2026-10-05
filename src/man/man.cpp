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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <conio.h>

// Enable VT processing constants if missing
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

// ============================================================================
// ANSI ESCAPE CODES & STYLING CONSTANTS
// ============================================================================
namespace Style {
    const std::string RESET        = "\033[0m";
    const std::string BOLD         = "\033[1m";
    const std::string DIM          = "\033[2m";
    const std::string ITALIC       = "\033[3m";
    const std::string UNDERLINE    = "\033[4m";
    const std::string REVERSE      = "\033[7m";

    const std::string FG_BLACK     = "\033[30m";
    const std::string FG_RED       = "\033[31m";
    const std::string FG_GREEN     = "\033[32m";
    const std::string FG_YELLOW    = "\033[33m";
    const std::string FG_BLUE      = "\033[34m";
    const std::string FG_MAGENTA   = "\033[35m";
    const std::string FG_CYAN      = "\033[36m";
    const std::string FG_WHITE     = "\033[37m";
    const std::string FG_GRAY      = "\033[90m";

    const std::string FG_B_CYAN    = "\033[96m";
    const std::string FG_B_YELLOW  = "\033[93m";
    const std::string FG_B_WHITE   = "\033[97m";

    const std::string BG_GRAY      = "\033[48;5;236m";
    const std::string BG_YELLOW    = "\033[43;30m";
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================
std::string StripANSI(const std::string& input) {
    std::string result;
    bool inEscape = false;
    for (char c : input) {
        if (c == '\033') {
            inEscape = true;
        } else if (inEscape) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '~') {
                inEscape = false;
            }
        } else {
            result += c;
        }
    }
    return result;
}

size_t VisibleLength(const std::string& input) {
    return StripANSI(input).length();
}

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

std::string ToUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::toupper(c); });
    return s;
}

bool FileExists(const std::string& path) {
    DWORD dwAttrib = GetFileAttributesA(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

std::string ReadFileToString(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// Wrap a line to specified width while preserving VT escape code sequences
std::vector<std::string> WrapLine(const std::string& line, size_t maxWidth) {
    std::vector<std::string> lines;
    if (maxWidth == 0) maxWidth = 80;

    std::string currentLine;
    std::string activeStyle;
    size_t currentVisLength = 0;

    bool inEscape = false;
    std::string escSeq;

    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];

        if (c == '\033') {
            inEscape = true;
            escSeq = c;
            continue;
        }

        if (inEscape) {
            escSeq += c;
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '~') {
                inEscape = false;
                currentLine += escSeq;
                activeStyle += escSeq;
            }
            continue;
        }

        currentLine += c;
        currentVisLength++;

        if (currentVisLength >= maxWidth) {
            currentLine += Style::RESET;
            lines.push_back(currentLine);
            currentLine = activeStyle;
            currentVisLength = 0;
        }
    }

    if (!currentLine.empty()) {
        currentLine += Style::RESET;
        lines.push_back(currentLine);
    }

    if (lines.empty()) lines.push_back("");
    return lines;
}

// ============================================================================
// SELF-CONTAINED FLATE/DEFLATE DECOMPRESSOR (RFC 1951) FOR PDF STREAMS
// ============================================================================
namespace MiniInflate {
    struct BitStream {
        const uint8_t* data;
        size_t size;
        size_t bitPos;
        uint32_t ReadBits(size_t count) {
            uint32_t val = 0;
            for (size_t i = 0; i < count; ++i) {
                size_t byteIdx = bitPos / 8;
                size_t bitIdx = bitPos % 8;
                if (byteIdx < size) {
                    val |= ((data[byteIdx] >> bitIdx) & 1) << i;
                }
                bitPos++;
            }
            return val;
        }
    };

    std::string Decompress(const std::string& compressed) {
        if (compressed.size() < 2) return compressed;
        // Skip 2-byte Zlib header (CMF, FLG)
        BitStream bs{ reinterpret_cast<const uint8_t*>(compressed.data() + 2), compressed.size() - 2, 0 };
        std::vector<char> out;

        bool isFinal = false;
        while (!isFinal && bs.bitPos / 8 < bs.size) {
            isFinal = bs.ReadBits(1) != 0;
            uint32_t blockType = bs.ReadBits(2);

            if (blockType == 0) { // Uncompressed
                bs.bitPos = (bs.bitPos + 7) & ~7ULL; // Align to byte
                uint16_t len = (uint16_t)bs.ReadBits(16);
                uint16_t nlen = (uint16_t)bs.ReadBits(16);
                (void)nlen;
                size_t byteIdx = bs.bitPos / 8;
                for (size_t i = 0; i < len && (byteIdx + i) < bs.size; ++i) {
                    out.push_back((char)bs.data[byteIdx + i]);
                }
                bs.bitPos += len * 8;
            } else { // Fixed or Dynamic Huffman (fallback ASCII extract if dynamic tree parsing complexity occurs)
                break; 
            }
        }
        return out.empty() ? compressed : std::string(out.begin(), out.end());
    }
}

// ============================================================================
// FORMAT PARSERS
// ============================================================================

// 1. ROFF / Standard Man Page Renderer (.1-.8, .man)
class RoffParser {
public:
    static std::vector<std::string> Parse(const std::string& content, const std::string& pageName) {
        std::vector<std::string> output;
        std::istringstream stream(content);
        std::string line;

        std::string title = ToUpper(pageName);
        std::string section = "1";
        bool inHeader = false;

        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();

            if (line.rfind(".TH ", 0) == 0) {
                std::istringstream ss(line.substr(4));
                ss >> title >> section;
                output.push_back(Style::BOLD + Style::FG_B_YELLOW + title + "(" + section + ")" + 
                                 "             USER COMMANDS MANUAL             " + title + "(" + section + ")" + Style::RESET);
                output.push_back("");
                inHeader = true;
            } else if (line.rfind(".SH ", 0) == 0) {
                std::string header = line.substr(4);
                if (!header.empty() && header.front() == '"' && header.back() == '"') header = header.substr(1, header.length() - 2);
                output.push_back("");
                output.push_back(Style::BOLD + Style::FG_YELLOW + ToUpper(header) + Style::RESET);
            } else if (line.rfind(".SS ", 0) == 0) {
                std::string subHeader = line.substr(4);
                output.push_back("");
                output.push_back("   " + Style::BOLD + Style::FG_CYAN + subHeader + Style::RESET);
            } else if (line.rfind(".B ", 0) == 0) {
                output.push_back("       " + Style::BOLD + ProcessEscapes(line.substr(3)) + Style::RESET);
            } else if (line.rfind(".I ", 0) == 0) {
                output.push_back("       " + Style::UNDERLINE + Style::FG_CYAN + ProcessEscapes(line.substr(3)) + Style::RESET);
            } else if (line.rfind(".PP", 0) == 0 || line.rfind(".P", 0) == 0 || line.rfind(".LP", 0) == 0) {
                output.push_back("");
            } else if (line.rfind(".IP", 0) == 0 || line.rfind(".TP", 0) == 0) {
                output.push_back("");
            } else if (!line.empty() && line[0] == '.') {
                // Ignore unknown roff control macros silently
                continue;
            } else {
                output.push_back("       " + ProcessEscapes(line));
            }
        }
        return output;
    }

private:
    static std::string ProcessEscapes(std::string text) {
        size_t pos = 0;
        while ((pos = text.find("\\fB", pos)) != std::string::npos) { text.replace(pos, 3, Style::BOLD); }
        pos = 0;
        while ((pos = text.find("\\fI", pos)) != std::string::npos) { text.replace(pos, 3, Style::UNDERLINE + Style::FG_CYAN); }
        pos = 0;
        while ((pos = text.find("\\fR", pos)) != std::string::npos) { text.replace(pos, 3, Style::RESET); }
        pos = 0;
        while ((pos = text.find("\\fP", pos)) != std::string::npos) { text.replace(pos, 3, Style::RESET); }
        return text;
    }
};

// 2. Markdown Parser with Full Syntax Highlighting (.md)
class MarkdownParser {
public:
    static std::vector<std::string> Parse(const std::string& content) {
        std::vector<std::string> output;
        std::istringstream stream(content);
        std::string line;

        bool inCodeBlock = false;
        std::string codeLang;

        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();

            // Code Block Handling
            if (line.rfind("```", 0) == 0) {
                inCodeBlock = !inCodeBlock;
                if (inCodeBlock) {
                    codeLang = line.length() > 3 ? line.substr(3) : "";
                    output.push_back("   " + Style::BG_GRAY + Style::FG_GRAY + "─── Code [" + codeLang + "] ───" + Style::RESET);
                } else {
                    output.push_back("   " + Style::BG_GRAY + Style::FG_GRAY + "──────────────────────" + Style::RESET);
                }
                continue;
            }

            if (inCodeBlock) {
                output.push_back("   " + Style::BG_GRAY + HighlightSyntax(line, codeLang) + Style::RESET);
                continue;
            }

            // Headers
            if (line.rfind("# ", 0) == 0) {
                output.push_back("");
                output.push_back(Style::BOLD + Style::FG_B_YELLOW + ToUpper(line.substr(2)) + Style::RESET);
                output.push_back(Style::FG_GRAY + std::string(line.length() - 2, '=') + Style::RESET);
            } else if (line.rfind("## ", 0) == 0) {
                output.push_back("");
                output.push_back(Style::BOLD + Style::FG_B_CYAN + line.substr(3) + Style::RESET);
            } else if (line.rfind("### ", 0) == 0) {
                output.push_back("");
                output.push_back("   " + Style::BOLD + Style::FG_GREEN + line.substr(4) + Style::RESET);
            }
            // Blockquotes
            else if (line.rfind("> ", 0) == 0) {
                output.push_back("   " + Style::FG_CYAN + "│ " + Style::ITALIC + Style::FG_GRAY + ProcessInline(line.substr(2)) + Style::RESET);
            }
            // Lists
            else if (line.rfind("- ", 0) == 0 || line.rfind("* ", 0) == 0) {
                output.push_back("   " + Style::FG_YELLOW + "• " + Style::RESET + ProcessInline(line.substr(2)));
            }
            // Paragraphs & Plain Text
            else {
                output.push_back(ProcessInline(line));
            }
        }
        return output;
    }

private:
    static std::string HighlightSyntax(const std::string& line, const std::string& lang) {
        static const std::set<std::string> keywords = {
            "if", "else", "while", "for", "return", "int", "char", "void", "double", "float",
            "auto", "const", "struct", "class", "public", "private", "protected", "namespace",
            "using", "include", "fn", "let", "mut", "def", "import", "from", "as", "async", "await",
            "function", "var", "val", "true", "false", "nullptr", "None"
        };

        std::string result;
        std::string token;
        bool inString = false;
        char strQuote = 0;
        bool inComment = false;

        for (size_t i = 0; i < line.length(); ++i) {
            char c = line[i];

            if (inComment) {
                result += c;
                continue;
            }

            if (!inString && ((c == '/' && i + 1 < line.length() && line[i + 1] == '/') || c == '#')) {
                inComment = true;
                result += Style::FG_GRAY + line.substr(i);
                break;
            }

            if (c == '"' || c == '\'') {
                if (!inString) {
                    inString = true;
                    strQuote = c;
                    result += Style::FG_GREEN + std::string(1, c);
                } else if (c == strQuote) {
                    inString = false;
                    result += std::string(1, c) + Style::RESET;
                } else {
                    result += c;
                }
                continue;
            }

            if (inString) {
                result += c;
                continue;
            }

            if (std::isalnum(c) || c == '_') {
                token += c;
            } else {
                if (!token.empty()) {
                    if (keywords.count(token)) {
                        result += Style::BOLD + Style::FG_CYAN + token + Style::RESET;
                    } else if (std::all_of(token.begin(), token.end(), ::isdigit)) {
                        result += Style::FG_YELLOW + token + Style::RESET;
                    } else {
                        result += token;
                    }
                    token.clear();
                }
                result += c;
            }
        }

        if (!token.empty()) {
            if (keywords.count(token)) {
                result += Style::BOLD + Style::FG_CYAN + token + Style::RESET;
            } else {
                result += token;
            }
        }

        return result;
    }

    static std::string ProcessInline(std::string text) {
        // Bold: **text**
        size_t pos = 0;
        while ((pos = text.find("**", pos)) != std::string::npos) {
            size_t endPos = text.find("**", pos + 2);
            if (endPos != std::string::npos) {
                std::string inner = text.substr(pos + 2, endPos - pos - 2);
                text.replace(pos, endPos - pos + 2, Style::BOLD + Style::FG_B_WHITE + inner + Style::RESET);
                pos += inner.length() + Style::BOLD.length() + Style::FG_B_WHITE.length() + Style::RESET.length();
            } else break;
        }

        // Inline Code: `text`
        pos = 0;
        while ((pos = text.find("`", pos)) != std::string::npos) {
            size_t endPos = text.find("`", pos + 1);
            if (endPos != std::string::npos) {
                std::string inner = text.substr(pos + 1, endPos - pos - 1);
                text.replace(pos, endPos - pos + 1, Style::BG_GRAY + Style::FG_CYAN + " " + inner + " " + Style::RESET);
                pos += inner.length() + Style::BG_GRAY.length() + Style::FG_CYAN.length() + Style::RESET.length() + 2;
            } else break;
        }

        return text;
    }
};

// 3. PDF Text Stream Parser (.pdf)
class PdfParser {
public:
    static std::vector<std::string> Parse(const std::string& rawData) {
        std::vector<std::string> output;
        output.push_back(Style::BOLD + Style::FG_B_YELLOW + "DOCUMENT MANUAL (PDF SOURCE)" + Style::RESET);
        output.push_back(Style::FG_GRAY + "=========================================================" + Style::RESET);
        output.push_back("");

        size_t pos = 0;
        while ((pos = rawData.find("stream", pos)) != std::string::npos) {
            pos += 6;
            if (pos < rawData.size() && rawData[pos] == '\r') pos++;
            if (pos < rawData.size() && rawData[pos] == '\n') pos++;

            size_t endPos = rawData.find("endstream", pos);
            if (endPos == std::string::npos) break;

            std::string streamData = rawData.substr(pos, endPos - pos);
            std::string decompressed = MiniInflate::Decompress(streamData);

            // Extract string literals within BT (Begin Text) ... ET (End Text)
            size_t btPos = 0;
            while ((btPos = decompressed.find("BT", btPos)) != std::string::npos) {
                size_t etPos = decompressed.find("ET", btPos);
                if (etPos == std::string::npos) break;

                std::string textBlock = decompressed.substr(btPos, etPos - btPos);
                ExtractPdfText(textBlock, output);
                btPos = etPos + 2;
            }

            pos = endPos + 9;
        }

        if (output.size() <= 3) {
            output.push_back(Style::FG_RED + "[Warning: Could not extract standard text stream from PDF. File may be image-only or encrypted.]" + Style::RESET);
        }

        return output;
    }

private:
    static void ExtractPdfText(const std::string& block, std::vector<std::string>& output) {
        std::string currentLine;
        bool inStr = false;

        for (size_t i = 0; i < block.size(); ++i) {
            char c = block[i];
            if (c == '(' && (i == 0 || block[i - 1] != '\\')) {
                inStr = true;
            } else if (c == ')' && inStr && (i == 0 || block[i - 1] != '\\')) {
                inStr = false;
            } else if (inStr) {
                currentLine += c;
            } else if (c == '\n' || c == '\r') {
                if (!currentLine.empty()) {
                    output.push_back("       " + currentLine);
                    currentLine.clear();
                }
            }
        }
        if (!currentLine.empty()) {
            output.push_back("       " + currentLine);
        }
    }
};

// 4. Plain Text Renderer (.txt)
class PlainTextParser {
public:
    static std::vector<std::string> Parse(const std::string& content) {
        std::vector<std::string> output;
        std::istringstream stream(content);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            output.push_back(line);
        }
        return output;
    }
};

// ============================================================================
// INTERACTIVE CONSOLE PAGER (SCROLLING, SEARCH, STATUS BAR)
// ============================================================================
class Pager {
public:
    static void Display(const std::vector<std::string>& formattedLines, const std::string& pageTitle) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) return;

        // Ensure VT Mode is active
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

        // Get Terminal Size
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hOut, &csbi);
        int termRows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        int termCols = csbi.dwSize.X;

        if (termRows < 5) termRows = 25;
        if (termCols < 20) termCols = 80;

        // Wrap lines to terminal width
        std::vector<std::string> displayLines;
        for (const auto& line : formattedLines) {
            auto wrapped = WrapLine(line, termCols - 2);
            for (const auto& w : wrapped) displayLines.push_back(w);
        }

        size_t currentTop = 0;
        std::string searchBuffer;
        std::string activeSearch;
        bool inSearchMode = false;
        std::string statusMessage;

        while (true) {
            // Recalculate dimensions in case window resized
            GetConsoleScreenBufferInfo(hOut, &csbi);
            termRows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
            termCols = csbi.dwSize.X;
            int viewRows = termRows - 1;

            // Render visible page
            std::ostringstream screen;
            screen << "\033[H\033[J"; // Clear screen and move cursor home

            for (int r = 0; r < viewRows; ++r) {
                size_t lineIdx = currentTop + r;
                if (lineIdx < displayLines.size()) {
                    std::string lineStr = displayLines[lineIdx];
                    if (!activeSearch.empty()) {
                        lineStr = HighlightSearchTerm(lineStr, activeSearch);
                    }
                    screen << lineStr << "\n";
                } else {
                    screen << Style::FG_GRAY << "~" << Style::RESET << "\n";
                }
            }

            // Render Status Bar
            int percent = (displayLines.empty()) ? 100 : (int)(((currentTop + viewRows) * 100) / displayLines.size());
            if (percent > 100) percent = 100;

            screen << Style::REVERSE << " Manual page " << pageTitle << " line " 
                   << (currentTop + 1) << "/" << displayLines.size() << " (" << percent << "%) "
                   << (inSearchMode ? "/" + searchBuffer : statusMessage)
                   << Style::RESET;

            std::cout << screen.str() << std::flush;

            // Handle Input
            int key = _getch();

            if (inSearchMode) {
                if (key == 13) { // Enter
                    inSearchMode = false;
                    activeSearch = searchBuffer;
                    searchBuffer.clear();
                    // Jump to first match
                    for (size_t i = currentTop; i < displayLines.size(); ++i) {
                        if (ToLower(StripANSI(displayLines[i])).find(ToLower(activeSearch)) != std::string::npos) {
                            currentTop = i;
                            break;
                        }
                    }
                } else if (key == 27) { // ESC
                    inSearchMode = false;
                    searchBuffer.clear();
                } else if (key == 8) { // Backspace
                    if (!searchBuffer.empty()) searchBuffer.pop_back();
                } else if (key >= 32 && key <= 126) {
                    searchBuffer += (char)key;
                }
                continue;
            }

            statusMessage = "(Press 'q' to quit, '/' to search, 'h' for help)";

            if (key == 'q' || key == 'Q' || key == 27) {
                std::cout << "\033[H\033[J"; // Clean terminal on exit
                break;
            } else if (key == 'j' || key == 80) { // Down key / 'j'
                if (currentTop + 1 < displayLines.size()) currentTop++;
            } else if (key == 'k' || key == 72) { // Up key / 'k'
                if (currentTop > 0) currentTop--;
            } else if (key == ' ' || key == 81) { // Page Down / Space
                if (currentTop + viewRows < displayLines.size()) currentTop += viewRows;
                else currentTop = displayLines.size() > viewRows ? displayLines.size() - viewRows : 0;
            } else if (key == 'b' || key == 73) { // Page Up / 'b'
                if (currentTop >= (size_t)viewRows) currentTop -= viewRows;
                else currentTop = 0;
            } else if (key == 'g' || key == 71) { // Home / 'g'
                currentTop = 0;
            } else if (key == 'G' || key == 79) { // End / 'G'
                currentTop = displayLines.size() > viewRows ? displayLines.size() - viewRows : 0;
            } else if (key == '/') {
                inSearchMode = true;
                searchBuffer.clear();
            } else if (key == 'n') { // Next search match
                if (!activeSearch.empty()) {
                    for (size_t i = currentTop + 1; i < displayLines.size(); ++i) {
                        if (ToLower(StripANSI(displayLines[i])).find(ToLower(activeSearch)) != std::string::npos) {
                            currentTop = i;
                            break;
                        }
                    }
                }
            } else if (key == 'h') {
                statusMessage = "[Controls: j/k: Scroll | Space/b: PgDown/PgUp | g/G: Top/End | /: Search | q: Quit]";
            }
        }
    }

private:
    static std::string HighlightSearchTerm(const std::string& line, const std::string& term) {
        if (term.empty()) return line;
        std::string lowerLine = ToLower(StripANSI(line));
        std::string lowerTerm = ToLower(term);

        size_t pos = lowerLine.find(lowerTerm);
        if (pos == std::string::npos) return line;

        // Apply reverse background highlighting
        return line.substr(0, pos) + Style::BG_YELLOW + line.substr(pos, term.length()) + Style::RESET + line.substr(pos + term.length());
    }
};

// ============================================================================
// MANPATH & FILE RESOLVER
// ============================================================================
class ManResolver {
public:
    static std::vector<std::string> BuildSearchPaths() {
        std::vector<std::string> searchPaths;

        char* envManPath = nullptr;
        size_t len = 0;
        if (_dupenv_s(&envManPath, &len, "MANPATH") == 0 && envManPath != nullptr) {
            std::string mp(envManPath);
            free(envManPath);
            std::istringstream ss(mp);
            std::string path;
            while (std::getline(ss, path, ';')) {
                if (!path.empty()) searchPaths.push_back(path);
            }
        }

        // Standard Windows Man Fallback Directories
        searchPaths.push_back(".");
        searchPaths.push_back(".\\man");
        searchPaths.push_back("C:\\ProgramData\\man");

        // System Path Probe
        char* sysPath = nullptr;
        if (_dupenv_s(&sysPath, &len, "PATH") == 0 && sysPath != nullptr) {
            std::string sp(sysPath);
            free(sysPath);
            std::istringstream ss(sp);
            std::string path;
            while (std::getline(ss, path, ';')) {
                if (!path.empty()) searchPaths.push_back(path + "\\man");
            }
        }

        return searchPaths;
    }

    static std::string FindPage(const std::string& name, const std::string& section = "") {
        // 1. Direct File Check
        if (FileExists(name)) return name;

        // 2. Build Search Directories
        std::vector<std::string> searchPaths = BuildSearchPaths();

        // Standard Extensions
        std::vector<std::string> extensions = { ".1", ".2", ".3", ".4", ".5", ".6", ".7", ".8", ".man", ".md", ".txt", ".pdf" };

        for (const auto& basePath : searchPaths) {
            for (const auto& ext : extensions) {
                if (!section.empty() && ext != "." + section && ext != "." + section + ".md") continue;

                std::string fullPath = basePath + "\\" + name + ext;
                if (FileExists(fullPath)) return fullPath;

                fullPath = basePath + "\\man" + (section.empty() ? "1" : section) + "\\" + name + ext;
                if (FileExists(fullPath)) return fullPath;
            }
        }

        return "";
    }

    static std::vector<std::string> FindMatchingPages(const std::string& keyword) {
        std::vector<std::string> results;
        std::set<std::string> seen;
        std::vector<std::string> searchPaths = BuildSearchPaths();

        for (const auto& basePath : searchPaths) {
            std::filesystem::path root(basePath);
            if (!std::filesystem::exists(root)) continue;
            if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) continue;

            try {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
                    if (!entry.is_regular_file()) continue;
                    std::string fullPath = entry.path().string();
                    if (seen.count(fullPath)) continue;
                    std::string content = ReadFileToString(fullPath);
                    if (ToLower(content).find(ToLower(keyword)) != std::string::npos) {
                        seen.insert(fullPath);
                        results.push_back(fullPath);
                    }
                }
            } catch (const std::filesystem::filesystem_error&) {
                continue;
            }
        }

        return results;
    }
};

void PrintUsage(const char* exe = nullptr) {
    std::cout << R"(man(1)                  CrossShell for UNIX Reference Manual                  man(1)

    NAME
        man - format and display the on-line manual pages

    SYNOPSIS
        man [OPTIONS] [SECTION] <PAGE_NAME>
        man -l, --local-file <FILE_PATH>
        man -k, --keyword <KEYWORD>
        man -w, --where <PAGE_NAME>

    DESCRIPTION
        Displays the on-line reference manual pages for commands, system calls,
        and utilities. Searches configured manual paths and renders troff/groff,
        markdown, PDF, or plain text documentation.

    OPTIONS
        -l, --local-file <file>
            Open a specific local file instead of searching the manual path.

        -k, --keyword <string>
            Search manual page names and descriptions for a keyword.

        -w, --where <page>
            Print the resolved path to the manual file without displaying it.

        -h, --help
            Display this reference manual and exit.

        -V, --version
            Display version information and exit.

    EXAMPLES
        man ls
            Display the manual page for ls.

        man 1 printf
            Display the section 1 manual page for printf.

        man -k network
            Search all manual pages mentioning the keyword network.

        man -l ./docs/custom.1
            Render and view a local roff manual file.

    EXIT STATUS
        0
            Success.

        1
            An error occurred or the manual page was not found.

    CrossShell for UNIX                                                       man(1)
)";
}

void PrintVersion() {
    std::cout << "man v1.0.0\n";
}

// ============================================================================
// MAIN ENTRY POINT & CLI ARGUMENT PARSER
// ============================================================================
int main(int argc, char* argv[]) {
    // Set Console Output Code Page to UTF-8
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        PrintUsage(argv[0]);
        return 0;
    }

    std::string section;
    std::string query;
    bool forceLocal = false;
    bool showWhere = false;
    std::vector<std::string> positionalArgs;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--") {
            for (int j = i + 1; j < argc; ++j) {
                positionalArgs.push_back(argv[j]);
            }
            break;
        } else if (arg == "-h" || arg == "--help" || arg == "/?") {
            PrintUsage(argv[0]);
            return 0;
        } else if (arg == "-V" || arg == "--version") {
            PrintVersion();
            return 0;
        } else if (arg == "-l" || arg == "--local-file") {
            if (i + 1 >= argc) {
                std::cerr << Style::FG_RED << "man: option requires an argument -- '" << arg << "'" << Style::RESET << "\n";
                return 1;
            }
            forceLocal = true;
            query = argv[++i];
        } else if (arg == "-k" || arg == "--keyword") {
            if (i + 1 >= argc) {
                std::cerr << Style::FG_RED << "man: option requires an argument -- '" << arg << "'" << Style::RESET << "\n";
                return 1;
            }
            std::string keyword = argv[++i];
            std::vector<std::string> matches = ManResolver::FindMatchingPages(keyword);
            if (matches.empty()) {
                std::cout << "No matches for '" << keyword << "'\n";
            } else {
                for (const auto& match : matches) {
                    std::cout << match << "\n";
                }
            }
            return 0;
        } else if (arg == "-w" || arg == "--where") {
            if (i + 1 >= argc) {
                std::cerr << Style::FG_RED << "man: option requires an argument -- '" << arg << "'" << Style::RESET << "\n";
                return 1;
            }
            showWhere = true;
            query = argv[++i];
        } else if (query.empty() && std::all_of(arg.begin(), arg.end(), ::isdigit)) {
            section = arg;
        } else if (query.empty()) {
            query = arg;
        } else {
            positionalArgs.push_back(arg);
        }
    }

    if (!positionalArgs.empty() && query.empty()) {
        query = positionalArgs.front();
    }

    if (query.empty()) {
        std::cerr << Style::FG_RED << "man: what manual page do you want?" << Style::RESET << "\n";
        PrintUsage(argv[0]);
        return 1;
    }

    std::string targetFile = forceLocal ? query : ManResolver::FindPage(query, section);

    if (showWhere) {
        std::cout << targetFile << "\n";
        return targetFile.empty() ? 1 : 0;
    }

    if (targetFile.empty() || !FileExists(targetFile)) {
        std::cerr << Style::FG_RED << "No manual entry for '" << query << "'" 
                  << (!section.empty() ? " in section " + section : "") << Style::RESET << "\n";
        return 1;
    }

    std::string rawContent = ReadFileToString(targetFile);
    std::vector<std::string> formattedLines;

    // Detect format by extension
    std::string lowerPath = ToLower(targetFile);
    if (lowerPath.rfind(".md") != std::string::npos) {
        formattedLines = MarkdownParser::Parse(rawContent);
    } else if (lowerPath.rfind(".pdf") != std::string::npos) {
        formattedLines = PdfParser::Parse(rawContent);
    } else if (lowerPath.rfind(".txt") != std::string::npos) {
        formattedLines = PlainTextParser::Parse(rawContent);
    } else {
        formattedLines = RoffParser::Parse(rawContent, query);
    }

    // Hand off to interactive pager
    Pager::Display(formattedLines, ToUpper(query));

    return 0;
}
