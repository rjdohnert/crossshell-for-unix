#pragma once

#include "pdh_fallback_state.hpp"
#include "pdh_rate_sample.hpp"
#include "vmstat.hpp"

class PdhFallbackEngine {
private:
    PdhFallbackState m_pdh;

    static bool AddPdhCounter(HQUERY query, const char* path, HCOUNTER& counter);

    static bool ReadPdhCounterDouble(HCOUNTER counter, double& value);

public:
    bool Init();

    bool CollectRates(PdhRateSample& sample);

    void Shutdown();

    bool IsInitialized() const;
};
