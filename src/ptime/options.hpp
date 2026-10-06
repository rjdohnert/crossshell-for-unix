#pragma once

#include "ptime.hpp"

class TimeOptions {
public:
    bool posixFormat = false;
    bool detailedInfo = false;
    bool humanReadable = false;
    std::wstring outputFile;
    bool appendOutput = false;
    bool showHelp = false;
    bool showVersion = false;
    int cmdStart = -1;

    bool Parse(int argc, wchar_t* argv[]);
    void PrintHelp() const;
    void PrintVersion() const;
};
