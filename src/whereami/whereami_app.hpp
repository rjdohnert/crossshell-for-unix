#pragma once

#include "whereami_options.hpp"
#include "whereami.hpp"

class WhereAmIApp {
private:
    CliOptions options;

public:
    int parse_arguments(int argc, char* argv[]);

    int execute();
};
