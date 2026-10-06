#include "engine.hpp"

std::string LineNumberFormatter::format(long long num, int width, NumberFormat fmt) {
    std::ostringstream ss;
    if (fmt == NumberFormat::LN) {
        ss << std::left << std::setw(width) << num;
    } else if (fmt == NumberFormat::RN) {
        ss << std::right << std::setw(width) << num;
    } else if (fmt == NumberFormat::RZ) {
        ss << std::right << std::setw(width) << std::setfill('0') << num;
    }
    return ss.str();
}

bool NlEngine::shouldNumber(const std::string& line, const NlStyle& style, int& blankCount, int blankLimit) {
    if (style.mode == StyleMode::NONE) return false;
    if (style.mode == StyleMode::ALL) {
        if (line.empty()) {
            blankCount++;
            if (blankCount >= blankLimit) {
                blankCount = 0;
                return true;
            }
            return false;
        }
        blankCount = 0;
        return true;
    }
    if (style.mode == StyleMode::NON_EMPTY) {
        return !line.empty();
    }
    if (style.mode == StyleMode::REGEX) {
        return std::regex_search(line, style.pattern);
    }
    return false;
}

void NlEngine::processStream(std::istream& in) const {
    std::string line;
    long long currentNum = options.startNum;
    SectionType currentSec = SectionType::BODY;
    int blankCount = 0;

    std::string hDelim = options.delim + options.delim + options.delim;
    std::string bDelim = options.delim + options.delim;
    std::string fDelim = options.delim;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line == hDelim) {
            currentSec = SectionType::HEADER;
            if (options.renumberPerPage) currentNum = options.startNum;
            std::cout << "\n";
            continue;
        } else if (line == bDelim) {
            currentSec = SectionType::BODY;
            if (options.renumberPerPage) currentNum = options.startNum;
            std::cout << "\n";
            continue;
        } else if (line == fDelim) {
            currentSec = SectionType::FOOTER;
            if (options.renumberPerPage) currentNum = options.startNum;
            std::cout << "\n";
            continue;
        }

        const NlStyle* activeStyle = &options.bodyStyle;
        if (currentSec == SectionType::HEADER) activeStyle = &options.headerStyle;
        else if (currentSec == SectionType::FOOTER) activeStyle = &options.footerStyle;

        if (shouldNumber(line, *activeStyle, blankCount, options.blankLinesLimit)) {
            std::cout << LineNumberFormatter::format(currentNum, options.width, options.format)
                      << options.separator << line << "\n";
            currentNum += options.increment;
        } else {
            std::cout << std::string(options.width, ' ') << options.separator << line << "\n";
        }
    }
}

NlEngine::NlEngine(NlOptions opts) : options(std::move(opts)) {}

int NlEngine::execute() {
    for (const auto& file : options.files) {
        if (file == "-") {
            processStream(std::cin);
        } else {
            std::ifstream infile(file);
            if (!infile.is_open()) {
                std::cerr << "nl: " << file << ": No such file or directory\n";
                return 1;
            }
            processStream(infile);
        }
    }
    return 0;
}
