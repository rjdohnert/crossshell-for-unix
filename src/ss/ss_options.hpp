#pragma once

#include "ss.hpp"

class SsOptions {
public:
    bool showTcp = true;
    bool showUdp = true;
    bool listeningOnly = false;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]);

    void PrintUsage(const wchar_t* programName) const;

    void PrintVersion() const;
};
