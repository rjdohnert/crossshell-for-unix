#ifndef RUNCON_OPTIONS_HPP
#define RUNCON_OPTIONS_HPP

#include "runcon.hpp"

class RunconOptions {
public:
    std::wstring level;
    std::vector<std::wstring> commandArgs;

    static void printUsage(const wchar_t* progName);
    static void printVersion();
    static bool parse(int argc, wchar_t* argv[], RunconOptions& opts);
};

#endif // RUNCON_OPTIONS_HPP
