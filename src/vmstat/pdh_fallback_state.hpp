#pragma once

#include "vmstat.hpp"

struct PdhFallbackState {
    HQUERY query = nullptr;
    HCOUNTER pageFaults = nullptr;
    HCOUNTER pagesIn = nullptr;
    HCOUNTER pagesOut = nullptr;
    HCOUNTER interrupts = nullptr;
    HCOUNTER systemCalls = nullptr;
    HCOUNTER contextSwitches = nullptr;
    bool initialized = false;
};
