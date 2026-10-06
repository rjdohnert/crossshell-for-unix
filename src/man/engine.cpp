#include "engine.hpp"

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
        BitStream bs{ reinterpret_cast<const uint8_t*>(compressed.data() + 2), compressed.size() - 2, 0 };
        std::vector<char> out;

        bool isFinal = false;
        while (!isFinal && bs.bitPos / 8 < bs.size) {
            isFinal = bs.ReadBits(1) != 0;
            uint32_t blockType = bs.ReadBits(2);

            if (blockType == 0) {
                bs.bitPos = (bs.bitPos + 7) & ~7ULL;
                uint16_t len = (uint16_t)bs.ReadBits(16);
                uint16_t nlen = (uint16_t)bs.ReadBits(16);
                (void)nlen;
                size_t byteIdx = bs.bitPos / 8;
                for (size_t i = 0; i < len && (byteIdx + i) < bs.size; ++i) {
                    out.push_back((char)bs.data[byteIdx + i]);
                }
                bs.bitPos += len * 8;
            } else {
                break; 
            }
        }
        return out.empty() ? compressed : std::string(out.begin(), out.end());
    }
}

// ============================================================================
// FORMAT PARSERS
// ============================================================================
std::vector<std::string> RoffParser::Parse(const std::string& content, const std::string& pageName) {
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
            continue;
        } else {
            output.push_back("       " + ProcessEscapes(line));
        }
    }
    return output;
}

std::string RoffParser::ProcessEscapes(std::string text) {
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

std::vector<std::string> MarkdownParser::Parse(const std::string& content) {
    std::vector<std::string> output;
    std::istringstream stream(content);
    std::string line;

    bool inCodeBlock = false;
    std::string codeLang;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

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
        } else if (line.rfind("> ", 0) == 0) {
            output.push_back("   " + Style::FG_CYAN + "│ " + Style::ITALIC + Style::FG_GRAY + ProcessInline(line.substr(2)) + Style::RESET);
        } else if (line.rfind("- ", 0) == 0 || line.rfind("* ", 0) == 0) {
            output.push_back("   " + Style::FG_YELLOW + "• " + Style::RESET + ProcessInline(line.substr(2)));
        } else {
            output.push_back(ProcessInline(line));
        }
    }
    return output;
}

std::string MarkdownParser::HighlightSyntax(const std::string& line, const std::string& lang) {
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

std::string MarkdownParser::ProcessInline(std::string text) {
    size_t pos = 0;
    while ((pos = text.find("**", pos)) != std::string::npos) {
        size_t endPos = text.find("**", pos + 2);
        if (endPos != std::string::npos) {
            std::string inner = text.substr(pos + 2, endPos - pos - 2);
            text.replace(pos, endPos - pos + 2, Style::BOLD + Style::FG_B_WHITE + inner + Style::RESET);
            pos += inner.length() + Style::BOLD.length() + Style::FG_B_WHITE.length() + Style::RESET.length();
        } else break;
    }

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

std::vector<std::string> PdfParser::Parse(const std::string& rawData) {
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

void PdfParser::ExtractPdfText(const std::string& block, std::vector<std::string>& output) {
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

std::vector<std::string> PlainTextParser::Parse(const std::string& content) {
    std::vector<std::string> output;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        output.push_back(line);
    }
    return output;
}

// ============================================================================
// MANPATH & FILE RESOLVER
// ============================================================================
std::vector<std::string> ManResolver::BuildSearchPaths() {
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

    searchPaths.push_back(".");
    searchPaths.push_back(".\\man");
    searchPaths.push_back("C:\\ProgramData\\man");

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

std::string ManResolver::FindPage(const std::string& name, const std::string& section) {
    if (FileExists(name)) return name;

    std::vector<std::string> searchPaths = BuildSearchPaths();
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

std::vector<std::string> ManResolver::FindMatchingPages(const std::string& keyword) {
    std::vector<std::string> results;
    std::set<std::string> seen;
    std::vector<std::string> searchPaths = BuildSearchPaths();

    for (const auto& basePath : searchPaths) {
        std::filesystem::path root(basePath);
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

// ============================================================================
// INTERACTIVE CONSOLE PAGER
// ============================================================================
void Pager::Display(const std::vector<std::string>& formattedLines, const std::string& pageTitle) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hOut, &csbi);
    int termRows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    int termCols = csbi.dwSize.X;

    if (termRows < 5) termRows = 25;
    if (termCols < 20) termCols = 80;

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
        GetConsoleScreenBufferInfo(hOut, &csbi);
        termRows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        termCols = csbi.dwSize.X;
        int viewRows = termRows - 1;

        std::ostringstream screen;
        screen << "\033[H\033[J";

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

        int percent = (displayLines.empty()) ? 100 : (int)(((currentTop + viewRows) * 100) / displayLines.size());
        if (percent > 100) percent = 100;

        screen << Style::REVERSE << " Manual page " << pageTitle << " line " 
               << (currentTop + 1) << "/" << displayLines.size() << " (" << percent << "%) "
               << (inSearchMode ? "/" + searchBuffer : statusMessage)
               << Style::RESET;

        std::cout << screen.str() << std::flush;

        int key = _getch();

        if (inSearchMode) {
            if (key == 13) {
                inSearchMode = false;
                activeSearch = searchBuffer;
                searchBuffer.clear();
                for (size_t i = currentTop; i < displayLines.size(); ++i) {
                    if (ToLower(StripANSI(displayLines[i])).find(ToLower(activeSearch)) != std::string::npos) {
                        currentTop = i;
                        break;
                    }
                }
            } else if (key == 27) {
                inSearchMode = false;
                searchBuffer.clear();
            } else if (key == 8) {
                if (!searchBuffer.empty()) searchBuffer.pop_back();
            } else if (key >= 32 && key <= 126) {
                searchBuffer += (char)key;
            }
            continue;
        }

        statusMessage = "(Press 'q' to quit, '/' to search, 'h' for help)";

        if (key == 'q' || key == 'Q' || key == 27) {
            std::cout << "\033[H\033[J";
            break;
        } else if (key == 'j' || key == 80) {
            if (currentTop + 1 < displayLines.size()) currentTop++;
        } else if (key == 'k' || key == 72) {
            if (currentTop > 0) currentTop--;
        } else if (key == ' ' || key == 81) {
            if (currentTop + viewRows < displayLines.size()) currentTop += viewRows;
            else currentTop = displayLines.size() > viewRows ? displayLines.size() - viewRows : 0;
        } else if (key == 'b' || key == 73) {
            if (currentTop >= (size_t)viewRows) currentTop -= viewRows;
            else currentTop = 0;
        } else if (key == 'g' || key == 71) {
            currentTop = 0;
        } else if (key == 'G' || key == 79) {
            currentTop = displayLines.size() > viewRows ? displayLines.size() - viewRows : 0;
        } else if (key == '/') {
            inSearchMode = true;
            searchBuffer.clear();
        } else if (key == 'n') {
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

std::string Pager::HighlightSearchTerm(const std::string& line, const std::string& term) {
    if (term.empty()) return line;
    std::string lowerLine = ToLower(StripANSI(line));
    std::string lowerTerm = ToLower(term);

    size_t pos = lowerLine.find(lowerTerm);
    if (pos == std::string::npos) return line;

    return line.substr(0, pos) + Style::BG_YELLOW + line.substr(pos, term.length()) + Style::RESET + line.substr(pos + term.length());
}
