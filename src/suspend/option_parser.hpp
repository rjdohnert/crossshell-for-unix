#pragma once

#include "suspend_options.hpp"
#include "suspend.hpp"

class OptionParser {
public:
    static void PrintUsage(const wchar_t* program_name);

    static void PrintVersion();

    SuspendOptions Parse(int argc, wchar_t* argv[]) const;
};
