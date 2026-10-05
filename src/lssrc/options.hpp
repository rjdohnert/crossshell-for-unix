#pragma once

#include "lssrc.hpp"
#include <string>
#include <vector>

class LssrcOptions {
public:
    bool includeStopped = false;
    bool showHelp = false;
    bool showVersion = false;
    std::wstring specificService;
    LssrcFormat format = LssrcFormat::Table;
    std::vector<std::wstring> filters;

    bool Parse(int argc, wchar_t* argv[]);
    void PrintUsage(const char* prog = "lssrc") const;
    void PrintVersion() const;
};
