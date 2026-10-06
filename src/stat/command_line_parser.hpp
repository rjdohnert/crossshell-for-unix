#pragma once

#include "stat_options.hpp"
#include "stat.hpp"

class CommandLineParser {
public:
    static ProgramOptions parse(int argc, wchar_t* argv[]);
};
