#pragma once

#include "wc_options.hpp"
#include "wc.hpp"

class OptionParser {
public:
    static void PrintUsage();

    static void PrintVersion();

    bool Parse(int argc, char* argv[], WcOptions& opts, bool& exitEarly) const;
};
