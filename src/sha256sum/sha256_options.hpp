#ifndef SHA256_OPTIONS_HPP
#define SHA256_OPTIONS_HPP

#include "sha256sum.hpp"

class Sha256Options {
public:
    bool binaryMode{true};
    bool doCheck{false};
    bool quiet{false};
    bool status{false};
    bool warn{false};
    std::vector<std::string> files;
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage(const char* progName);
    static void printVersion();
    static bool parse(int argc, char* argv[], Sha256Options& opts);
};

#endif // SHA256_OPTIONS_HPP
