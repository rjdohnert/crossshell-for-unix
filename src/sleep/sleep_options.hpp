#ifndef SLEEP_OPTIONS_HPP
#define SLEEP_OPTIONS_HPP

#include "sleep.hpp"

class SleepOptionsParser {
public:
    static void printHelp();
    static void printVersion();
    static bool parse(int argc, wchar_t* argv[], SleepOptions& options);
};

#endif // SLEEP_OPTIONS_HPP
