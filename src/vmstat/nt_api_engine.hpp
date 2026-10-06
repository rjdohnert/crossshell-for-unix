#pragma once

#include "system_sample.hpp"
#include "vmstat.hpp"

class NtApiEngine {
private:
    pfnNtQuerySystemInformation m_ntQuery = nullptr;

public:
    bool Init();

    bool TakeSample(SystemSample& sample);

    static double ClampRate(double value);

    static double ComputeDeltaRate(ULONGLONG curr, ULONGLONG prev, double dt);

    static const char* SelectSourceTag(bool ntAvailable, bool pdhAvailable);
};
