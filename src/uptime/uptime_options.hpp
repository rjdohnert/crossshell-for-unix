#pragma once

#include "uptime.hpp"

class UptimeOptions {
public:
    bool pretty{false};
    bool since{false};

    static void printUsage();

    static void printVersion();

    static bool parse(int argc, char* argv[], UptimeOptions& opts);
};
