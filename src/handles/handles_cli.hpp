#pragma once

#include "handles_types.hpp"

#include <string>

class HandlesCli {
public:
    static bool parse(int argc, wchar_t* argv[], AppConfig& config,
                      std::wstring& error);
    static void printHelp();
    static void printVersion();
};
