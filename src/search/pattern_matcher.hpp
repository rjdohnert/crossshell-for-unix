#ifndef PATTERN_MATCHER_HPP
#define PATTERN_MATCHER_HPP

#include "search.hpp"

class PatternMatcher {
private:
    std::unique_ptr<std::regex> m_regex;

    static std::string WildcardToRegex(const std::string& pattern, bool exact);

public:
    bool Initialize(const std::string& pattern, bool isRegex, bool exact, bool caseInsensitive);
    bool HasPattern() const;
    bool Matches(const std::string& filename) const;
};

#endif // PATTERN_MATCHER_HPP
