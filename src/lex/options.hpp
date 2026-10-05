#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "lex.hpp"
#include <string>

class CommandLineParser {
public:
    static void printHelp();
    static void printVersion();
    static FlexOptions parse(int argc, wchar_t* argv[]);
};

#endif // OPTIONS_HPP
