#pragma once

#include "options.hpp"

class LsusbApp {
private:
    CommandLineOptions options;
public:
    explicit LsusbApp(CommandLineOptions opts);
    int run();
};
