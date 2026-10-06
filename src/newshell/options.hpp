#ifndef NEWSHELL_OPTIONS_HPP
#define NEWSHELL_OPTIONS_HPP

#include "newshell.hpp"

class NewshellOptionsParser {
public:
    static bool Parse(int argc, wchar_t* argv[], NewshellOptions& opts);
    static void PrintHelp();
    static void PrintVersion();
};

#endif // NEWSHELL_OPTIONS_HPP
