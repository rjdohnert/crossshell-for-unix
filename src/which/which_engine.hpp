#pragma once

#include "which_options.hpp"
#include "which.hpp"

class WhichEngine {
private:
    WhichOptions options;

public:
    explicit WhichEngine(WhichOptions opts);

    int execute();
};
