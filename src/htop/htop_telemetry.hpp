#pragma once

#include "htop_types.hpp"

class TelemetryEngine {
public:
    double totalCpuPercent = 0.0;
    std::vector<CpuCoreData> cores;

    SIZE_T totalRam = 0;
    SIZE_T availRam = 0;
    SIZE_T usedRam = 0;

    SIZE_T commitLimit = 0;
    SIZE_T commitTotal = 0;

    ULONG totalTasks = 0;
    ULONG totalThreads = 0;
    ULONG runningTasks = 0;

    ULONGLONG uptimeSeconds = 0;
    std::chrono::steady_clock::time_point lastUpdateTime{};
    uint64_t currentGeneration = 0;
    std::wstring lastError;
    std::unordered_map<DWORD, ProcessItem> processes;

    void Initialize();
    void UpdateSystemMetrics();
};
