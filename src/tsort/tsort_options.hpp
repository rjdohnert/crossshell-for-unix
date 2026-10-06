#pragma once

#include "tsort.hpp"

class TsortOptions {
public:
    bool multiple{false};
    std::vector<std::string> filenames;
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage();

    static void printVersion();

    static bool parse(int argc, char* argv[], TsortOptions& opts);
};
