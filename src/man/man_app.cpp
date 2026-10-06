#include "man_app.hpp"

int ManApp::run(const ManOptions& opts) {
    if (opts.showHelp) {
        PrintUsage();
        return 0;
    }
    if (opts.showVersion) {
        PrintVersion();
        return 0;
    }

    if (opts.keywordSearch) {
        std::vector<std::string> matches = ManResolver::FindMatchingPages(opts.keyword);
        if (matches.empty()) {
            std::cout << "No matches for '" << opts.keyword << "'\n";
        } else {
            for (const auto& match : matches) {
                std::cout << match << "\n";
            }
        }
        return 0;
    }

    if (opts.query.empty()) {
        std::cerr << Style::FG_RED << "man: what manual page do you want?" << Style::RESET << "\n";
        PrintUsage();
        return 1;
    }

    std::string targetFile = opts.forceLocal ? opts.query : ManResolver::FindPage(opts.query, opts.section);

    if (opts.showWhere) {
        std::cout << targetFile << "\n";
        return targetFile.empty() ? 1 : 0;
    }

    if (targetFile.empty() || !FileExists(targetFile)) {
        std::cerr << Style::FG_RED << "No manual entry for '" << opts.query << "'" 
                  << (!opts.section.empty() ? " in section " + opts.section : "") << Style::RESET << "\n";
        return 1;
    }

    std::string rawContent = ReadFileToString(targetFile);
    std::vector<std::string> formattedLines;

    std::string lowerPath = ToLower(targetFile);
    if (lowerPath.rfind(".md") != std::string::npos) {
        formattedLines = MarkdownParser::Parse(rawContent);
    } else if (lowerPath.rfind(".pdf") != std::string::npos) {
        formattedLines = PdfParser::Parse(rawContent);
    } else if (lowerPath.rfind(".txt") != std::string::npos) {
        formattedLines = PlainTextParser::Parse(rawContent);
    } else {
        formattedLines = RoffParser::Parse(rawContent, opts.query);
    }

    Pager::Display(formattedLines, ToUpper(opts.query));
    return 0;
}
