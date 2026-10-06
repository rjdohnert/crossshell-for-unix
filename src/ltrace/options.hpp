#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "ltrace.hpp"

class CommandLineParser {
public:
    static void showHelp();
    static void showVersion();
    static bool parse(int argc, char* argv[], Config& config);
};

#endif // OPTIONS_HPP
