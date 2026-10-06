#pragma once

#include "tee_options.hpp"
#include "tee.hpp"

class TeeEngine {
public:
    static bool Process(const TeeOptions& opts);
};
