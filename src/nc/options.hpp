#ifndef NC_OPTIONS_HPP
#define NC_OPTIONS_HPP

#include "nc.hpp"

struct NcOptions {
    bool ipv4_only = false;
    bool ipv6_only = false;
    bool detach_stdin = false;
    bool listen = false;
    bool keep_open = false;
    bool no_dns = false;
    bool udp = false;
    int verbose = 0;
    bool zero_io = false;
    std::string exec_cmd;
    int interval = 0;
    int timeout = 0;
    int local_port = 0;
    std::string source_ip;
    std::string destination;
    std::vector<int> ports;
    bool showHelp = false;
    bool showVersion = false;

    void ParsePorts(const std::string& arg);
    bool Parse(int argc, char* argv[]);
    void PrintHelp() const;
    void PrintVersion() const;
};

#endif // NC_OPTIONS_HPP
