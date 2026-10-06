#pragma once

#include "rm_options.hpp"
#include "rm.hpp"

class OptionParser {
public:
    static void ShowHelp();

    static void ShowVersion();

    bool Parse(int argc, char* argv[], RmOptions& opts) const;
};
