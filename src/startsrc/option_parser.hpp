#pragma once

#include "startsrc_options.hpp"
#include "startsrc.hpp"

class OptionParser {
public:
    static void PrintHelp();

    static void PrintVersion();

    bool Parse(int argc, wchar_t* argv[], StartsrcOptions& opt) const;
};
