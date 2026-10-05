#pragma once

#include <string>

struct CommandLineOptions {
    bool verbose = false;
    bool readStdin = false;
    std::string format = "classic";
    int filterBus = -1;
    int filterDev = -1;
    std::string filterVid;
    std::string filterPid;
};

class CommandLineParser {
public:
    static void printHelp();
    static CommandLineOptions parse(int argc, char* argv[]);
};
