#ifndef NL_OPTIONS_HPP
#define NL_OPTIONS_HPP

#include "nl.hpp"

class NlOptions {
public:
    NlStyle bodyStyle{StyleMode::NON_EMPTY, {}, ""};
    NlStyle headerStyle{StyleMode::NONE, {}, ""};
    NlStyle footerStyle{StyleMode::NONE, {}, ""};

    std::string delim{"\\:"};
    long long startNum{1};
    long long increment{1};
    bool renumberPerPage{true};
    int blankLinesLimit{1};
    std::string separator{"\t"};
    int width{6};
    NumberFormat format{NumberFormat::RN};
    std::vector<std::string> files;

    static void printHelp();
    static void printVersion();
    static bool parse(int argc, char* argv[], NlOptions& opts);
};

#endif // NL_OPTIONS_HPP
