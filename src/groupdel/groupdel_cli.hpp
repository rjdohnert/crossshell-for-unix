#pragma once

#include "groupdel_types.hpp"

class CommandLineParser {
public:
    static bool Parse(int argc, wchar_t* argv[], CommandOptions& options,
                      std::wstring& error);
    static void PrintHelp();
    static void PrintVersion();
};
