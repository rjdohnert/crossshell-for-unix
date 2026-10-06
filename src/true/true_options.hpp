#pragma once

#include "true.hpp"

class TrueOptions {
public:
    int outputFormat{0}; // 0: none, 1: JSON, 2: CSV, 3: Table
    std::wstring pipeCommand;

    static void printHelp();

    static void printVersion();

    static bool parse(int argc, wchar_t* argv[], TrueOptions& opts);
};
