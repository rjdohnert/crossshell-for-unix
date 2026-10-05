#pragma once

#include "lscfg.hpp"
#include <string>

class LscfgOptions {
public:
    bool verbose = false;
    bool summaryOnly = false;
    std::string filter = "";
    OutputFormat format = OutputFormat::Table;

    bool Parse(int argc, char* argv[]);
    static void PrintHelp(const char* exeName);
    static void PrintVersion();
};
