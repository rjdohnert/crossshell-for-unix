#ifndef MV_OPTIONS_HPP
#define MV_OPTIONS_HPP

#include "mv.hpp"

class HelpFormatter {
public:
    static void printHelp();
    static void printVersion();
};

class ArgumentParser {
public:
    static bool parse(int argc, char* argv[], MoveOptions& options);
};

#endif // MV_OPTIONS_HPP
