#pragma once

#include "lsblk.hpp"
#include <string>
#include <vector>

class LsblkOptions {
public:
    bool noHeadings = false;
    OutputFormat format = OutputFormat::Table;
    std::vector<std::wstring> filters;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]);
    void PrintHelp(const wchar_t* progName) const;
    void PrintVersion() const;
};
