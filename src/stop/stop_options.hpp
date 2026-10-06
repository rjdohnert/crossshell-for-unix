#pragma once

#include "stop.hpp"

class StopOptions {
public:
    SignalType signal = SignalType::SIGTERM;
    std::vector<DWORD> pids;
    OutputFormat outputFormat = OutputFormat::None;
    std::string pipeCommand;
    bool showHelp = false;
    bool showVersion = false;
    bool showSignalList = false;

    int Parse(int argc, char* argv[]);

    void PrintUsage() const;

    void PrintVersion() const;
};
