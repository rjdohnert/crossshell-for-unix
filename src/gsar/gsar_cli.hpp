#pragma once

#include "gsar_types.hpp"

#include <string>

class CommandLineParser {
public:
    static bool parse(int argc, char* argv[], Options& options, std::string& error);
    static void printHelp(const std::string& executableName);
};
