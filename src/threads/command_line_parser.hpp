#pragma once

#include "threads_options.hpp"
#include "threads.hpp"

class CommandLineParser {
public:
    static AppConfig parse(int argc, wchar_t* argv[]);

    static void printHelp();

    static void printVersion();
};
