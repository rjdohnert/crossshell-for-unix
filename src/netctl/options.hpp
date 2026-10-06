#ifndef NETCTL_OPTIONS_HPP
#define NETCTL_OPTIONS_HPP

#include "netctl.hpp"

enum class NetctlCommand {
    None,
    Help,
    Version,
    List,
    Capture,
    Unknown
};

struct NetctlOptions {
    NetctlCommand command = NetctlCommand::None;
    Config config;
    std::string unknown_command = "";
    std::string error_message = "";
    bool valid = true;
};

class NetctlOptionsParser {
public:
    static NetctlOptions Parse(int argc, char* argv[]);
    static void DisplayHelp();
    static void DisplayVersion();

private:
    static bool ParsePortValue(const std::string& text, uint16_t& out_port);
    static bool ParsePacketCount(const std::string& text, uint32_t& out_count);
};

#endif // NETCTL_OPTIONS_HPP
