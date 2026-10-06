#pragma once

#include "tty.hpp"

class TtyOptions {
public:
    bool silent{false};

    static void printHelp();

    static void printVersion();

    static bool parse(int argc, wchar_t* argv[], TtyOptions& opts);
};
