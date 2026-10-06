#ifndef MKFIFO_OPTIONS_HPP
#define MKFIFO_OPTIONS_HPP

#include "mkfifo.hpp"

class HelpFormatter {
public:
    static void printHelp();
    static void printVersion();
};

class ArgumentParser {
public:
    static bool parse(int argc, char* argv[], FifoOptions& opts);
};

#endif // MKFIFO_OPTIONS_HPP
