#ifndef SEARCH_ENGINE_HPP
#define SEARCH_ENGINE_HPP

#include "search.hpp"
#include "pattern_matcher.hpp"
#include "terminal_format.hpp"
#include "search_reporter.hpp"

class SearchEngine {
private:
    PatternMatcher m_matcher;

public:
    int Execute(const SearchOptions& opt);
};

#endif // SEARCH_ENGINE_HPP
