#ifndef SCRIPT_OPTIONS_HPP
#define SCRIPT_OPTIONS_HPP

#include "script.hpp"

class ScriptOptions {
public:
    bool append{false};
    std::wstring outputFile{L"typescript"};
    std::vector<std::wstring> commandArgs;

    static void printUsage(const wchar_t* progName);
    static void printVersion();
    static bool parse(int argc, wchar_t* argv[], ScriptOptions& opts);
};

#endif // SCRIPT_OPTIONS_HPP
