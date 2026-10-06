#pragma once

#include "vmstat.hpp"

struct VmstatOptions {
    UnitMode mode = UnitMode::Kilobytes;
    OutputMode outputMode = OutputMode::Table;
    LayoutMode layoutMode = LayoutMode::Auto;
    bool summaryMode = false;
    int interval = 0;
    int maxCount = -1;
    int headerInterval = 20;
    std::vector<std::string> positionalArgs;
};
