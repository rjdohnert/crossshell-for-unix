#pragma once

#include "cpu_snapshot.hpp"
#include "process_record.hpp"
#include "top.hpp"

struct TopOptions {
    int refreshDelaySec = 1;
    int maxDisplayCount = 20;
    SortMode currentSort = SortMode::CPU;
    std::string userFilter;
};

struct ProcessCpuSample {
    uint64_t cpuTime = 0;
    uint64_t creationTime = 0;
};

class HpUxTopEngine {
private:
    double load1 = 0.0, load5 = 0.0, load15 = 0.0;
    CpuSnapshot prevSystemCpu{};
    std::unordered_map<DWORD, ProcessCpuSample> prevProcTimes;
    std::chrono::steady_clock::time_point lastSampleTime;

    int refreshDelaySec;
    int maxDisplayCount;
    SortMode currentSort;
    std::string userFilter;
    bool running = true;
    HANDLE consoleOutput = INVALID_HANDLE_VALUE;
    DWORD originalConsoleMode = 0;
    bool consoleModeSaved = false;
    bool cursorHidden = false;

    // Helper: Convert FILETIME to 100ns units
    static uint64_t FileTimeToUint64(const FILETIME& ft);

    std::string FormatTime(uint64_t totalSec);

    std::string FormatBytes(uint64_t kb);

    std::string GetProcessUser(HANDLE hProcess);

public:
    explicit HpUxTopEngine(const TopOptions& options);

    ~HpUxTopEngine();

    bool EnableVirtualTerminal();

    void InitSystemSnapshot();

    void RenderHeader(const std::vector<ProcessRecord>& procs, double cpuUser, double cpuSys, double cpuIdle);

    void RenderHelp();

    void Update();

    void HandleInput();

    void PromptLine(const std::string& msg);

    std::string ReadPromptLine(const std::string& msg);

    int Run();
};
