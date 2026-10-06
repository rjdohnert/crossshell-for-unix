#pragma once

#include "rcp.hpp"

class RcpOptions {
public:
    bool recursive = false;
    bool preserve = false;
    std::string port = DEFAULT_RCP_PORT;
    std::string src;
    std::string dst;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(const std::vector<std::string>& args);
    void PrintHelp(const std::string& exe_name) const;
    void PrintVersion() const;
};
