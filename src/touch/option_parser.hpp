#pragma once

#include "touch_options.hpp"
#include "touch.hpp"

class OptionParser {
public:
    static void PrintUsage();

    bool Parse(int argc, wchar_t* argv[], TouchOptions& opts) const;
};
