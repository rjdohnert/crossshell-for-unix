#pragma once

#include "cat_formatter.hpp"
#include "cat_options.hpp"

class CatEngine {
private:
    CatOptions options;
    StreamProcessor processor;

public:
    explicit CatEngine(const CatOptions& opts);
    int execute();
};
