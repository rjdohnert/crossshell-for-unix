#pragma once

#include "renice.hpp"

class ReniceApplication {
public:
    ReniceApplication();

    void printHelp(std::string_view /*execName*/ = "renice") const;

    int run(int argc, char* argv[]);
};
