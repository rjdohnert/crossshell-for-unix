#pragma once

#include "zcat_options.hpp"
#include "zcat.hpp"

class OptionParser {
public:
    static void PrintUsage(const wchar_t* progName);

    static void PrintVersion();

    bool Parse(int argc, wchar_t* argv[], ZcatOptions& opts) const;
};
