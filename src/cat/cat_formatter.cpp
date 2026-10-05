#include "cat_formatter.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
class JsonSerializer {
public:
    static std::string escape(const std::string& str) {
        std::ostringstream output;
        for (unsigned char c : str) {
            switch (c) {
                case '"': output << "\\\""; break;
                case '\\': output << "\\\\"; break;
                case '\b': output << "\\b"; break;
                case '\f': output << "\\f"; break;
                case '\n': output << "\\n"; break;
                case '\r': output << "\\r"; break;
                case '\t': output << "\\t"; break;
                default:
                    if (c < 32) {
                        output << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                    } else {
                        output.put(static_cast<char>(c));
                    }
            }
        }
        return output.str();
    }
};
}

void CharacterFormatter::formatAndPrintChar(unsigned char c, bool showTabs, bool showNonPrinting) {
    if (c == '\t') {
        std::cout << (showTabs ? "^I" : "\t");
        return;
    }
    if (c == '\f') {
        std::cout << (showTabs ? "^L" : "\f");
        return;
    }
    if (!showNonPrinting) {
        std::cout.put(static_cast<char>(c));
        return;
    }
    if (c < 32 && c != '\n') {
        std::cout << '^' << static_cast<char>(c + 64);
    } else if (c == 127) {
        std::cout << "^?";
    } else if (c >= 128) {
        std::cout << "M-";
        unsigned char low7 = c & 0x7F;
        if (low7 < 32) {
            std::cout << '^' << static_cast<char>(low7 + 64);
        } else if (low7 == 127) {
            std::cout << "^?";
        } else {
            std::cout.put(static_cast<char>(low7));
        }
    } else {
        std::cout.put(static_cast<char>(c));
    }
}

StreamProcessor::StreamProcessor(const CatOptions& opts) : options(opts) {}

void StreamProcessor::printLineNumber() {
    std::cout << std::right << std::setw(6) << currentLineNumber++ << "\t";
}

void StreamProcessor::processRaw(std::istream& in) {
    constexpr size_t bufferSize = 64 * 1024;
    std::vector<char> buffer(bufferSize);
    while (in.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) || in.gcount() > 0) {
        std::cout.write(buffer.data(), in.gcount());
        if (options.unbuffered) {
            std::cout.flush();
        }
    }
}

void StreamProcessor::processFormatted(std::istream& in) {
    std::vector<unsigned char> lineBuffer;
    char ch;
    while (in.get(ch)) {
        unsigned char uc = static_cast<unsigned char>(ch);
        if (uc == '\r') {
            continue;
        }
        if (uc != '\n') {
            lineBuffer.push_back(uc);
            continue;
        }

        bool isBlank = lineBuffer.empty();
        if (options.squeezeBlanks && isBlank && previousWasBlank) {
            lineBuffer.clear();
            continue;
        }
        previousWasBlank = isBlank;

        if (options.outputJson) {
            std::string content(lineBuffer.begin(), lineBuffer.end());
            std::cout << "{\"line\":" << currentLineNumber << ",\"content\":\""
                      << JsonSerializer::escape(content) << "\"}\n";
            currentLineNumber++;
        } else {
            if (options.numberLines && !(options.numberNonBlank && isBlank)) {
                printLineNumber();
            }
            for (unsigned char c : lineBuffer) {
                CharacterFormatter::formatAndPrintChar(c, options.showTabs, options.showNonPrinting);
            }
            if (options.showEnds) {
                std::cout << '$';
            }
            std::cout.put('\n');
            if (options.unbuffered) {
                std::cout.flush();
            }
        }
        lineBuffer.clear();
    }

    if (!lineBuffer.empty()) {
        if (options.outputJson) {
            std::string content(lineBuffer.begin(), lineBuffer.end());
            std::cout << "{\"line\":" << currentLineNumber << ",\"content\":\""
                      << JsonSerializer::escape(content) << "\"}\n";
        } else {
            if (options.numberLines) {
                printLineNumber();
            }
            for (unsigned char c : lineBuffer) {
                CharacterFormatter::formatAndPrintChar(c, options.showTabs, options.showNonPrinting);
            }
            if (options.unbuffered) {
                std::cout.flush();
            }
        }
    }
}
