#pragma once

#include "whois.hpp"

class WhoisOptions {
public:
    std::string server;
    std::string port = "43";
    bool autoFollow = true;
    std::string target;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]);

    void PrintUsage(const char* progName) const;

    void PrintVersion() const;
};
