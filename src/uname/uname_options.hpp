#pragma once

#include "uname.hpp"

class UnameOptions {
public:
    bool printSysname{false};
    bool printNodename{false};
    bool printRelease{false};
    bool printVersion{false};
    bool printMachine{false};
    bool printId{false};
    bool printLicense{false};
    bool printModel{false};
    bool printExtended{false};
    std::wstring setHostname;
    int outputFormat{0};
    std::wstring pipeCommand;

    static void printHelp();

    static void printVersionHeader();

    static bool parse(int argc, wchar_t* argv[], UnameOptions& opts);
};
