#pragma once

#include "pstree.hpp"

class PstreeOptions {
public:
    static void PrintHelp();
    static void PrintVersion();
    static bool ParseCommandLine(int argc, wchar_t* argv[], Config& config);
};
