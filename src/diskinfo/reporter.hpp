#pragma once

#include "diskinfo.hpp"
#include "options.hpp"
#include <vector>

class DiskinfoReporter {
public:
    static void Report(const std::vector<BlockDeviceRow>& rows, const DiskinfoOptions& opts);
};
