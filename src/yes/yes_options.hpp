#pragma once

#include "yes.hpp"

class YesOptions {
public:
    static constexpr const char* VERSION = "1.0.0";
    std::string line{"y"};

    static void printHelp(const char* progName);

    static void printVersion();

    static bool parse(int argc, char* argv[], YesOptions& opts);
};
