#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "portmap.hpp"

class PortmapOptions {
public:
    bool showAll = false;
    bool numeric = false;
    bool showProcess = true;
    ProtocolFilter proto = ProtocolFilter::ALL;
    IpFilter ipVer = IpFilter::ALL;
    std::string stateFilter = "";
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]);
    void PrintHelp() const;
    void PrintVersion() const;
};

#endif // OPTIONS_HPP
