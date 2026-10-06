#ifndef SEARCH_OPTIONS_HPP
#define SEARCH_OPTIONS_HPP

#include "search.hpp"

class OptionParser {
public:
    static void PrintHelp();
    bool Parse(int argc, char* argv[], SearchOptions& opt) const;
};

#endif // SEARCH_OPTIONS_HPP
