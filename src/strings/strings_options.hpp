#pragma once

#include "strings.hpp"

class StringsOptions {
public:
    size_t minLength{4};
    std::vector<std::string> files;
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage(const char* progName);

    static void printVersion();

    static bool parseUnsigned(const std::string& text, size_t& outValue);

    static bool parse(int argc, char* argv[], StringsOptions& opts);
};
