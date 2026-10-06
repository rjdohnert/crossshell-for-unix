#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "make.hpp"

class CommandLineParser {
public:
    static void print_version();
    static void print_help();
    static bool parse_cli(int argc, char* argv[], Config& cfg);
};

#endif // OPTIONS_HPP
