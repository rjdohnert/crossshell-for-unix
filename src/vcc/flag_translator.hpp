#pragma once

#include "build_options.hpp"
#include "vcc.hpp"

class FlagTranslator {
public:
    static void PrintHelp(const std::string& exeName);

    static void PrintVersion();

    bool Parse(int argc, char* argv[], BuildOptions& opts) const;
};
