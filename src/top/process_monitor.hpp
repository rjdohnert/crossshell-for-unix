#pragma once

#include "cpu_snapshot.hpp"
#include "process_record.hpp"
#include "top.hpp"

class HpUxTopEngine {
private:
    double load1 = 0.0, load5 = 0.0, load15 = 0.0;
    CpuSnapshot prevSystemCpu{};
    std::unordered_map<DWORD, uint64_t> prevProcTimes;
    std::chrono::steady_clock::time_point lastSampleTime;

    int refreshDelaySec = 2;
    int maxDisplayCount = 20;
    SortMode currentSort = SortMode::CPU;
    std::string userFilter = "";
    bool showHelp = false;

    // Helper: Convert FILETIME to 100ns units
    static uint64_t FileTimeToUint64(const FILETIME& ft);

    std::string FormatTime(uint64_t totalSec);

    std::string FormatBytes(uint64_t kb);

    std::string GetProcessUser(HANDLE hProcess);

public:
    HpUxTopEngine();

    ~HpUxTopEngine();

    void EnableVirtualTerminal();

    void InitSystemSnapshot();

    void RenderHeader(const std::vector<ProcessRecord>& procs, double cpuUser, double cpuSys, double cpuIdle);

    void RenderHelp();

    void Update();

    void HandleInput();

    void PromptLine(const std::string& msg);

    void Run();
};
