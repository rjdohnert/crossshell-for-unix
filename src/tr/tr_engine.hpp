#pragma once

#include "tr_options.hpp"
#include "tr.hpp"

class TrEngine {
private:
    TrOptions options;

public:
    explicit TrEngine(TrOptions opts);

    int execute();
};
