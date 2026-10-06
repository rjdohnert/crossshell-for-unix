#pragma once

#include "sed.hpp"

class OptionParser {
public:
    static void DisplayHelp();
    static void DisplayVersion();
    bool Parse(int argc, char* argv[], SedOptions& opts, bool& exitEarly) const;
};
