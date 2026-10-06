#pragma once

#include "zgrep_options.hpp"
#include "zgrep.hpp"

class OptionParser {
public:
    static void PrintUsage(const wchar_t* progName);

    static void PrintVersion();

    bool Parse(int argc, wchar_t* argv[], ZgrepOptions& opts) const;
};
