#pragma once

#include "lsblk.hpp"
#include "options.hpp"
#include <vector>

class LsblkReporter {
public:
    static void Report(const std::vector<BlockDeviceRow>& rows, const LsblkOptions& opts);
};
