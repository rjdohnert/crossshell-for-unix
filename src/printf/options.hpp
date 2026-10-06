#pragma once

#include "printf.hpp"

struct PrintfOptions {
    bool show_help = false;
    bool show_version = false;
    std::string varName;
    std::string format;
    std::vector<std::string> args;
};

class OptionParser {
public:
    static void PrintHelp();
    static void PrintVersion();
    bool Parse(int argc, char* argv[], PrintfOptions& opts) const;
};
