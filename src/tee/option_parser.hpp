#pragma once

#include "tee_options.hpp"
#include "tee.hpp"

class OptionParser {
public:
    static void PrintHelp();

    static void PrintVersion();

    bool Parse(int argc, wchar_t* argv[], TeeOptions& opts, bool& exitEarly) const;
};
