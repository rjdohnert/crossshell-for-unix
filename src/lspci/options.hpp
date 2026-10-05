#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "lspci.hpp"
#include <string>

struct CommandLineOptions {
    bool verbose = false;
    bool showDrivers = false;
    bool readStdin = false;
    std::string format = "classic";
    std::optional<int> filterDom;
    std::optional<int> filterBus;
    std::optional<int> filterSlot;
    std::optional<int> filterFunc;
    std::string filterVid;
    std::string filterDid;
};

class CommandLineParser {
public:
    static void printHelp();
    static CommandLineOptions parse(int argc, char* argv[]);
};

#endif // OPTIONS_HPP
