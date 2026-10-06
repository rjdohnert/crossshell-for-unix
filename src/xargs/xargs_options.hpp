#pragma once

#include "xargs.hpp"

class XargsOptions {
public:
    DelimitMode mode{DelimitMode::WHITESPACE};
    int maxArgs{-1};
    int maxLines{-1};
    char delimiter{'\0'};
    bool verbose{false};
    bool noRunIfEmpty{false};
    std::string replaceStr;
    std::vector<std::string> command;

    static void printUsage(const char* progName);

    static void printVersion();

    static bool parse(int argc, char* argv[], XargsOptions& opts);
};
