#pragma once

#include "rename.hpp"

class HelpFormatter {
public:
    static void printUsage(std::ostream& os);

    static void printHelp(std::ostream& os);

    static void printVersion(std::ostream& os);
};
