#pragma once

#include "which.hpp"

class WhichOptions {
public:
    bool all{false};
    bool silent{false};
    std::vector<std::string> commands;
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage(const char* progName);

    static void printVersion();

    static bool parse(int argc, char* argv[], WhichOptions& opts);
};
