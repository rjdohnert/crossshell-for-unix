#pragma once

#include "prtconf.hpp"

struct SystemConfigSummary {
    std::string systemModel{"Windows Host"};
    std::string nodeName{"unknown"};
    std::string kernelArchitecture{"unknown"};
    DWORD numProcessors{0};
    uint64_t totalMemoryMb{0};
    std::string osLevel;
    DWORD totalDevices{0};
    std::map<std::string, int> classCounts;
};

class PrtconfOptions {
public:
    bool showDevices{false};

    static void printHelp();
    static void printVersion();
    static bool parse(int argc, wchar_t* argv[], PrtconfOptions& opt);
};
