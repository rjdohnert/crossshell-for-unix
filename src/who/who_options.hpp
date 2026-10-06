#pragma once

#include "who.hpp"

class WhoOptions {
public:
    bool optHeader = false;
    bool optCount = false;
    bool optBoot = false;
    bool optAmI = false;
    bool optAll = false;
    bool optLogin = false;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]);

    void PrintUsage(const wchar_t* progName) const;

    void PrintVersion() const;
};
