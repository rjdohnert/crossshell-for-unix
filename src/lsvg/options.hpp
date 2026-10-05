#pragma once

#include "lsvg.hpp"
#include <string>
#include <vector>

struct CmdOptions {
    bool listActiveOnly = false; // -o
    bool listLVs = false;        // -l
    bool listPVs = false;        // -p
    bool listAllDetail = false;  // -a
    std::string targetVG;
    LsvgFormat format = LsvgFormat::Table;
    std::vector<std::string> filters;

    bool Parse(int argc, char* argv[]);
    void PrintHelp() const;
    void PrintVersion() const;
};
