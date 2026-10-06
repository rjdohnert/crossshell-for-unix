#pragma once

#include "printenv.hpp"

class PrintenvOptions {
public:
    bool nullTerminated{false};
    std::vector<std::wstring> targets;

    static void printHelp();
    static void printVersion();
    static bool parse(int argc, wchar_t* argv[], PrintenvOptions& opts);
};
