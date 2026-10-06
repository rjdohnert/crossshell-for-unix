#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "pkill.hpp"

class PkillOptions {
public:
    bool ignoreCase = false;
    bool exact = false;
    bool echo = false;
    bool showHelp = false;
    bool showVersion = false;
    std::wstring pattern;

    int Parse(int argc, wchar_t* argv[]);
    void PrintUsage(const wchar_t* programName) const;
    void PrintVersion() const;
};

#endif // OPTIONS_HPP
