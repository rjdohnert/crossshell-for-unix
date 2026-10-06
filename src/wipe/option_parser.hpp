#pragma once

#include "wipe_options.hpp"
#include "wipe.hpp"

class OptionParser {
public:
    static void PrintUsage(const char* prog_name);

    bool Parse(int argc, char* argv[], WipeOptions& opts, bool& exitEarly) const;
};
