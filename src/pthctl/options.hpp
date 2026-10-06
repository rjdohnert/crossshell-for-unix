#pragma once

#include "pthctl.hpp"

struct PthctlParsedArgs {
    std::wstring command;
    Scope scope = Scope::User;
    bool prepend = false;
    bool dryRun = false;
    std::wstring pathArg;
    OutputFormat outputFormat = OutputFormat::Human;
    std::wstring pipeCommand;
    bool shouldExit = false;
    int exitCode = 0;
};

class PthctlOptions {
public:
    static void ShowVersion();
    static void ShowHelp();
    static bool Parse(int argc, wchar_t* argv[], PthctlParsedArgs& parsed);
};
