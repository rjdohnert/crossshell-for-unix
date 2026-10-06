#pragma once

#include "pwd.hpp"

class PwdOptions {
public:
    bool physical{false};

    static void printUsage();
    static void printVersion();
    static bool parse(int argc, wchar_t* argv[], PwdOptions& opts);
};
