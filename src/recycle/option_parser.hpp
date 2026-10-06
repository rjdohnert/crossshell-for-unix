#pragma once

#include "recycle_options.hpp"
#include "recycle.hpp"

class OptionParser {
public:
    static void PrintHelp();

    static void PrintVersion();

    RecycleOptions Parse(int argc, wchar_t* argv[]) const;
};
