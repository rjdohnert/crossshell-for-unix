#pragma once

#include "reboot.hpp"

class RebootOptions {
public:
    bool elevatedRun = false;
    bool suppressWall = false;
    bool force = false;
    bool wtmpOnly = false;
    bool showHelp = false;
    bool showVersion = false;

    int Parse(int argc, char* argv[]);

    void PrintUsage() const;

    void PrintVersion() const;
};
