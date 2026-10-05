#pragma once

#include "lsattr.hpp"
#include <string>
#include <vector>

class LsattrOptions {
public:
    bool recursive = false;
    bool showHelp = false;
    bool showVersion = false;
    OutputFormat format = OutputFormat::Table;
    std::vector<std::wstring> paths;

    bool Parse(int argc, wchar_t* argv[]);
    void PrintUsage(const wchar_t* progName) const;
    void PrintVersion() const;
};
