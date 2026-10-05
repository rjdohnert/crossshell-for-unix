#pragma once

#include "ln.hpp"

class OptionParser {
public:
    static void PrintUsage();
    static void PrintVersion();
    bool Parse(int argc, wchar_t* argv[], LnOptions& opts) const;
};
