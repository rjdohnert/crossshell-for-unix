#pragma once

#include "groupadd_types.hpp"

#include <string>

class CommandLineParser {
public:
    static bool Parse(int argc, wchar_t* argv[], CommandOptions& options,
                      std::wstring& error);
    static void PrintHelp();
    static void PrintVersion();
};
