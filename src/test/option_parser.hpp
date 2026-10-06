#pragma once

#include "test_options.hpp"
#include "test.hpp"

class OptionParser {
public:
    static void PrintHelp();

    static void PrintVersion();

    TestOptions Parse(int argc, wchar_t* argv[]) const;
};
