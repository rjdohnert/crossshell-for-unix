#pragma once

#include "strace.hpp"

class TraceOptions {
public:
    bool followChildren = false;
    bool timestamp = false;
    bool verbose = false;
    bool quiet = false;
    bool filterEvents = false;
    std::vector<std::wstring> eventFilters;
    bool attachToExistingProcess = false;
    DWORD targetPid = 0;
    bool summaryOnly = false;
    bool printIp = false;
    DWORD stringLimit = 32;
    OutputFormat outputFormat = OutputFormat::Human;
    bool jsonFirstRecord = true;
    std::wstring outputPath;
    std::wstring pipeCommand;
    std::vector<std::wstring> commandParts;

    bool Parse(int argc, wchar_t* argv[]);
    void PrintHelp() const;
    void PrintVersion() const;
};
