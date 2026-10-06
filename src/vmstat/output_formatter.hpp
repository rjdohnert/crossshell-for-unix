#pragma once

#include "nt_api_engine.hpp"
#include "pdh_fallback_engine.hpp"
#include "vmstat.hpp"

class OutputFormatter {
public:
    static void PrintHeader(UnitMode mode, LayoutMode layoutMode);

    static void PrintSummary(NtApiEngine& ntEngine, PdhFallbackEngine& pdhEngine);
};
