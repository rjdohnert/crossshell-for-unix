#pragma once

#include "whence_options.hpp"
#include "whence.hpp"

class WhenceEngine {
private:
    WhenceOptions options;

public:
    explicit WhenceEngine(WhenceOptions opts);

    int execute();
};
