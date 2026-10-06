#pragma once

#include "typeset_options.hpp"
#include "typeset.hpp"

class OptionParser {
public:
    static void PrintHelp();

    static void PrintVersion();

    bool Parse(int argc, wchar_t* argv[], TypesetOptions& opts) const;
};
