#pragma once

#include "lscfg.hpp"
#include "options.hpp"
#include <vector>

class LscfgReporter {
public:
    static void Report(const std::vector<HardwareDevice>& devices, const LscfgOptions& opts);
};
