#pragma once

#include <string>

struct CommandLineOptions {
    bool verbose = false;
    bool showDrivers = false;
    bool onlyConnected = false;
    bool readStdin = false;
    std::string format = "classic";
    int filterBus = -1;
    int filterSlot = -1;
    std::string filterMac;
};

class CommandLineParser {
public:
    static void printHelp();
    static CommandLineOptions parse(int argc, char* argv[]);
};
