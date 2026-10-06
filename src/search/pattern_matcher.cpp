#include "pattern_matcher.hpp"
#include "terminal_format.hpp"

std::string PatternMatcher::WildcardToRegex(const std::string& pattern, bool exact) {
    if (pattern.empty()) return ".*";
    std::string regexStr = exact ? "^" : "";

    bool hasWildcard = (pattern.find('*') != std::string::npos || pattern.find('?') != std::string::npos);
    if (!exact && !hasWildcard) {
        regexStr += ".*";
    }

    for (char c : pattern) {
        switch (c) {
            case '*': regexStr += ".*"; break;
            case '?': regexStr += "."; break;
            case '.': case '+': case '(': case ')':
            case '[': case ']': case '{': case '}':
            case '^': case '$': case '|': case '\\':
                regexStr += "\\";
                regexStr += c;
                break;
            default:
                regexStr += c;
        }
    }

    if (!exact && !hasWildcard) regexStr += ".*";
    if (exact) regexStr += "$";
    return regexStr;
}

bool PatternMatcher::Initialize(const std::string& pattern, bool isRegex, bool exact, bool caseInsensitive) {
    if (pattern.empty()) {
        m_regex.reset();
        return true;
    }

    std::string regStr = isRegex ? pattern : WildcardToRegex(pattern, exact);
    auto flags = std::regex::ECMAScript;
    if (caseInsensitive) flags |= std::regex::icase;

    try {
        m_regex = std::make_unique<std::regex>(regStr, flags);
        return true;
    } catch (const std::regex_error& e) {
        std::cerr << ConsoleTerminal::Red << "Regex compilation error: " << ConsoleTerminal::Reset << e.what() << "\n";
        return false;
    }
}

bool PatternMatcher::HasPattern() const {
    return m_regex != nullptr;
}

bool PatternMatcher::Matches(const std::string& filename) const {
    if (!m_regex) return true;
    return std::regex_match(filename, *m_regex);
}
