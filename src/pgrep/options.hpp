#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "pgrep.hpp"

struct PgrepOptions {
    bool ignoreCase = false;
    bool exact = false;
    bool listName = false;
    bool countOnly = false;
    bool showHelp = false;
    bool showVersion = false;
    std::wstring pattern;
};

class OptionParser {
public:
    static void PrintUsage(const wchar_t* programName);
    static void PrintVersion();
    bool Parse(int argc, wchar_t* argv[], PgrepOptions& options) const;
};

#endif // OPTIONS_HPP
