#pragma once

#include "xorriso_config.hpp"
#include "xorriso.hpp"

class OptionParser {
public:
    static void PrintHeader();

    static void PrintHelp();

    bool Parse(int argc, char* argv[], XorrisoConfig& cfg, bool& exitEarly) const;
};
