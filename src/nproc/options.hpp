#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "nproc.hpp"

class NprocOptions {
public:
    bool includeAll{false};
    unsigned long long ignore{0};

    static void printUsage(const char* programName);
    static void printVersion();
    static bool parseIgnoreValue(const std::string& text, unsigned long long& outValue);
    static bool parse(int argc, char* argv[], NprocOptions& opts);
};

#endif // OPTIONS_HPP
