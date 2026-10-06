#pragma once

#include "whereis_options.hpp"
#include "whereis.hpp"

class WhereisEngine {
private:
    WhereisOptions options;

public:
    explicit WhereisEngine(WhereisOptions opts);

    int execute();
};
