#pragma once

#include "tr.hpp"

class TrOptions {
public:
    bool complement{false};    // -c / -C
    bool deleteChars{false};   // -d
    bool squeeze{false};       // -s
    int outputFormat{0};       // 0: raw, 1: JSON, 2: CSV, 3: Table
    std::string pipeCommand;
    std::vector<std::string> args;

    static void printHelp();

    static bool parse(int argc, char* argv[], TrOptions& opts);
};
