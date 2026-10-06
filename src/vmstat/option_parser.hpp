#pragma once

#include "vmstat_options.hpp"
#include "vmstat.hpp"

class OptionParser {
public:
    static void ShowHelp();

    static void ShowVersion();

    bool Parse(int argc, wchar_t* argv[], VmstatOptions& opts, bool& exitEarly) const;
};
