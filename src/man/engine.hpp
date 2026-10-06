#ifndef MAN_ENGINE_HPP
#define MAN_ENGINE_HPP

#include "man.hpp"

namespace MiniInflate {
    std::string Decompress(const std::string& compressed);
}

class RoffParser {
public:
    static std::vector<std::string> Parse(const std::string& content, const std::string& pageName);
private:
    static std::string ProcessEscapes(std::string text);
};

class MarkdownParser {
public:
    static std::vector<std::string> Parse(const std::string& content);
private:
    static std::string HighlightSyntax(const std::string& line, const std::string& lang);
    static std::string ProcessInline(std::string text);
};

class PdfParser {
public:
    static std::vector<std::string> Parse(const std::string& rawData);
private:
    static void ExtractPdfText(const std::string& block, std::vector<std::string>& output);
};

class PlainTextParser {
public:
    static std::vector<std::string> Parse(const std::string& content);
};

class ManResolver {
public:
    static std::vector<std::string> BuildSearchPaths();
    static std::string FindPage(const std::string& name, const std::string& section = "");
    static std::vector<std::string> FindMatchingPages(const std::string& keyword);
};

class Pager {
public:
    static void Display(const std::vector<std::string>& formattedLines, const std::string& pageTitle);
private:
    static std::string HighlightSearchTerm(const std::string& line, const std::string& term);
};

#endif // MAN_ENGINE_HPP
