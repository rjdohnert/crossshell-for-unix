#pragma once

#include "watch.hpp"

class WatchOptions {
public:
    double intervalSeconds = 2.0;
    bool showHelp = false;
    bool showVersion = false;
    bool suppressHeader = false;
    std::vector<std::wstring> commandArgs;

    int Parse(int argc, wchar_t* argv[]);
};
