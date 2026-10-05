#pragma once

#include "options.hpp"

class LsbtApp {
private:
    CommandLineOptions options;
public:
    explicit LsbtApp(CommandLineOptions opts);
    int run();
};
