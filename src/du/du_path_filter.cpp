/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "du_path_filter.hpp"

#include <string_view>

namespace du {
namespace {

bool WildcardMatches(std::string_view pattern, std::string_view value) {
    std::size_t patternIndex = 0;
    std::size_t valueIndex = 0;
    std::size_t lastStar = std::string_view::npos;
    std::size_t starMatch = 0;

    while (valueIndex < value.size()) {
        if (patternIndex < pattern.size() &&
            (pattern[patternIndex] == '?' ||
             pattern[patternIndex] == value[valueIndex])) {
            ++patternIndex;
            ++valueIndex;
        } else if (patternIndex < pattern.size() &&
                   pattern[patternIndex] == '*') {
            lastStar = patternIndex++;
            starMatch = valueIndex;
        } else if (lastStar != std::string_view::npos) {
            patternIndex = lastStar + 1;
            valueIndex = ++starMatch;
        } else {
            return false;
        }
    }

    while (patternIndex < pattern.size() && pattern[patternIndex] == '*') {
        ++patternIndex;
    }
    return patternIndex == pattern.size();
}

} // namespace

bool IsPathExcluded(const std::string& name,
                    const std::vector<std::string>& patterns) {
    for (const std::string& pattern : patterns) {
        if (WildcardMatches(pattern, name)) return true;
    }
    return false;
}

} // namespace du
