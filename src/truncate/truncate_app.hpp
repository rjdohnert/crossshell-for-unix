#pragma once

#include "truncate_options.hpp"
#include "truncate.hpp"

class TruncateApp {
private:
    CliOptions options;

public:
    int parse(int argc, char* argv[]);

    int run();
};
