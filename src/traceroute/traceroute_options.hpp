#pragma once

#include "traceroute.hpp"

class TracerouteOptions {
public:
    int maxTtl = 30;
    int firstTtl = 1;
    int basePort = 33434;
    int nQueries = 3;
    int waitTimeSec = 3;
    bool numericMode = false;
    bool icmpMode = false;
    std::string srcIpStr;
    std::string targetHostStr;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]);

    void PrintHelp(const char* progName) const;

    void PrintVersion() const;
};
