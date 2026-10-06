#ifndef MEMINFO_OPTIONS_HPP
#define MEMINFO_OPTIONS_HPP

#include "meminfo.hpp"

class CommandLineParser {
public:
    static Config Parse(int argc, char* argv[]);
    static void PrintHelp();
};

#endif // MEMINFO_OPTIONS_HPP
