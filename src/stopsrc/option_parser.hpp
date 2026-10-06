#pragma once

#include "stopsrc_options.hpp"
#include "stopsrc.hpp"

class OptionParser {
public:
    static void PrintHelp();

    static void PrintVersion();

    bool Parse(int argc, wchar_t* argv[], StopsrcOptions& opt) const;
};
