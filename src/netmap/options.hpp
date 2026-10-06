#ifndef NETMAP_OPTIONS_HPP
#define NETMAP_OPTIONS_HPP

#include "netmap.hpp"

namespace Netmap {

struct NetmapOptions {
    Mode mode = Mode::Display;
    std::optional<std::string> targetIp;
    std::optional<std::string> targetMac;
    std::optional<std::string> ifFilter;
    std::optional<std::string> scanTarget;
    bool verbose = false;
    int timeoutMs = 800;
    int threadCount = 32;
    std::string helpTopic;
    bool valid = true;
    std::string errorMessage;
};

class HelpSystem {
public:
    static void ShowVersion();
    static void ShowHelp(const std::string& section = "");
    static void ShowAddHelp();
    static void ShowDeleteHelp();
    static void ShowScanHelp();
    static void ShowInterfaces();
};

class NetmapOptionsParser {
public:
    static NetmapOptions Parse(int argc, char* argv[]);
};

} // namespace Netmap

#endif // NETMAP_OPTIONS_HPP
