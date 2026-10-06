#pragma once

#include "tsort_options.hpp"
#include "tsort.hpp"

class TsortEngine {
private:
    TsortOptions options;

public:
    explicit TsortEngine(TsortOptions opts);

    int execute();
};
