#ifndef NODENAME_OPTIONS_HPP
#define NODENAME_OPTIONS_HPP

#include "nodename.hpp"

class NodenameOptions {
public:
    NameQueryMode mode = NameQueryMode::FullyQualified;
    std::wstring newHostname;
    bool hasNewHostname = false;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]);
    void PrintUsage(const wchar_t* progName) const;
    void PrintVersion() const;
};

#endif // NODENAME_OPTIONS_HPP
