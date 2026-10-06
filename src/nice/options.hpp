#ifndef NICE_OPTIONS_HPP
#define NICE_OPTIONS_HPP

#include "nice.hpp"

class NiceOptions {
public:
    int niceIncrement = 10;
    int cmdIndex = -1;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]);
    void PrintUsage(const wchar_t* exe) const;
    void PrintVersion() const;
};

#endif // NICE_OPTIONS_HPP
