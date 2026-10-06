#ifndef MAN_OPTIONS_HPP
#define MAN_OPTIONS_HPP

#include "man.hpp"

struct ManOptions {
    std::string section;
    std::string query;
    bool forceLocal = false;
    bool showWhere = false;
    bool keywordSearch = false;
    std::string keyword;
    bool showHelp = false;
    bool showVersion = false;
};

bool parse_options(int argc, char* argv[], ManOptions& opts);
void PrintUsage(const char* exe = nullptr);
void PrintVersion();

#endif // MAN_OPTIONS_HPP
