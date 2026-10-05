// ============================================================================
// SYSTEM TELEMETRY ENGINE
// ============================================================================
class TelemetryEngine {
public:
    double totalCpuPercent = 0.0;
    std::vector<CpuCoreData> cores;
    
    SIZE_T totalRam = 0;
    SIZE_T availRam = 0;
    SIZE_T usedRam = 0;
    
    SIZE_T totalPagefile = 0;
    SIZE_T usedPagefile = 0;
    
    ULONG totalTasks = 0;
    ULONG totalThreads = 0;
    ULONG runningTasks = 0;
    
    ULONGLONG uptimeSeconds = 0;
    ULONG processorQueueLength = 0;
    std::chrono::steady_clock::time_point lastUpdateTime{};
    uint64_t currentGeneration = 0;

    std::unordered_map<DWORD, ProcessItem> processes;

    void Initialize() {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        cores.resize(sysInfo.dwNumberOfProcessors);
        
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        if (GlobalMemoryStatusEx(&memStatus)) {
            totalRam = memStatus.ullTotalPhys;
        }

        UpdateSystemMetrics();
    }

    void UpdateSystemMetrics() {
        auto now = std::chrono::steady_clock::now();
        double elapsed100ns = 0.0;
        if (lastUpdateTime.time_since_epoch().count() > 0) {
            auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastUpdateTime).count();
            if (elapsedNs > 0) {
                elapsed100ns = static_cast<double>(elapsedNs) / 100.0;
            }
        }

        // 1. Uptime
        uptimeSeconds = GetTickCount64() / 1000;

        // 2. Memory & Pagefile Metrics
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        if (GlobalMemoryStatusEx(&memStatus)) {
            availRam = memStatus.ullAvailPhys;
            usedRam = totalRam - availRam;
        }

        PERFORMANCE_INFORMATION perfInfo;
        if (GetPerformanceInfo(&perfInfo, sizeof(perfInfo))) {
            totalPagefile = perfInfo.CommitLimit * perfInfo.PageSize;
            usedPagefile = perfInfo.CommitTotal * perfInfo.PageSize;
        }

        // 3. Per-Core CPU Usage
        ULONG returnLen = 0;
        std::vector<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION> perfBuffer(cores.size());
        NTSTATUS status = NtQuerySystemInformation(
            SystemProcessorPerformanceInformation,
            perfBuffer.data(),
            static_cast<ULONG>(sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * cores.size()),
            &returnLen
        );

        if (NT_SUCCESS(status)) {
            double accumCpu = 0.0;
            for (size_t i = 0; i < cores.size(); ++i) {
                ULONGLONG idle = perfBuffer[i].IdleTime.QuadPart;
                ULONGLONG kernel = perfBuffer[i].KernelTime.QuadPart;
                ULONGLONG user = perfBuffer[i].UserTime.QuadPart;

                ULONGLONG prevIdle = cores[i].idleTime;
                ULONGLONG prevKernel = cores[i].kernelTime;
                ULONGLONG prevUser = cores[i].userTime;

                ULONGLONG idleDelta = idle - prevIdle;
                ULONGLONG kernelDelta = kernel - prevKernel;
                ULONGLONG userDelta = user - prevUser;
                ULONGLONG totalDelta = kernelDelta + userDelta;

                cores[i].idleTime = idle;
                cores[i].kernelTime = kernel;
                cores[i].userTime = user;

                if (totalDelta > 0) {
                    cores[i].usagePercent = (1.0 - (double)idleDelta / (double)totalDelta) * 100.0;
                    if (cores[i].usagePercent < 0.0) cores[i].usagePercent = 0.0;
                    if (cores[i].usagePercent > 100.0) cores[i].usagePercent = 100.0;
                } else {
                    cores[i].usagePercent = 0.0;
                }
                accumCpu += cores[i].usagePercent;
            }
            totalCpuPercent = accumCpu / cores.size();
        }

        // 4. Processes & Threads via NtQuerySystemInformation
        ULONG procBufferLen = 1024 * 1024;
        std::vector<BYTE> procBuffer(procBufferLen);
        while (NtQuerySystemInformation(SystemProcessInformation, procBuffer.data(), procBufferLen, &returnLen) == (NTSTATUS)0xC0000004) {
            procBufferLen *= 2;
            procBuffer.resize(procBufferLen);
        }

        ULONG tasks = 0, threads = 0, running = 0;
        ++currentGeneration;

        SYSTEM_PROCESS_INFORMATION* pCurr = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(procBuffer.data());
        while (pCurr) {
            DWORD pid = HandleToUlong(pCurr->UniqueProcessId);
            DWORD ppid = HandleToUlong(pCurr->InheritedFromUniqueProcessId);

            auto [itProc, inserted] = processes.try_emplace(pid);
            ProcessItem& item = itProc->second;
            bool tagged = item.isTagged;
            ULONGLONG prevTotalTime = item.totalCpuTime;

            item.pid = pid;
            item.ppid = ppid;
            item.isTagged = tagged;
            
            if (pCurr->ImageName.Buffer != nullptr && pCurr->ImageName.Length > 0) {
                item.name = std::wstring(pCurr->ImageName.Buffer, pCurr->ImageName.Length / sizeof(wchar_t));
            } else {
                item.name = (pid == 0) ? L"System Idle Process" : ((pid == 4) ? L"System" : L"Unknown");
            }

            item.user = GetProcessOwner(pid);
            item.priority = pCurr->BasePriority;
            item.virtMem = pCurr->VirtualSize;
            item.resMem = pCurr->WorkingSetSize;
            item.privMem = pCurr->PrivatePageCount * 4096;
            item.threadCount = pCurr->NumberOfThreads;

            ULONGLONG totalTime = pCurr->KernelTime.QuadPart + pCurr->UserTime.QuadPart;
            item.totalCpuTime = totalTime;

            if (elapsed100ns > 0.0 && !inserted && !cores.empty()) {
                ULONGLONG deltaCpu = (totalTime >= prevTotalTime) ? (totalTime - prevTotalTime) : 0;
                double rawCpuPercent = (static_cast<double>(deltaCpu) / elapsed100ns) * 100.0;
                double normalized = rawCpuPercent / static_cast<double>(cores.size());
                item.cpuPercent = (std::max)(0.0, (std::min)(100.0, normalized));
            } else {
                item.cpuPercent = 0.0;
            }

            item.memPercent = (totalRam > 0) ? ((double)item.resMem / (double)totalRam) * 100.0 : 0.0;

            // Check running threads
            bool isRunning = false;
            for (ULONG t = 0; t < pCurr->NumberOfThreads; ++t) {
                if (pCurr->Threads[t].ThreadState == 2) { // 2 = Running
                    isRunning = true;
                    break;
                }
            }
            item.state = isRunning ? L'R' : L'S';

            tasks++;
            threads += item.threadCount;
            if (isRunning) running++;
            item.seenGeneration = currentGeneration;

            if (pCurr->NextEntryOffset == 0) break;
            pCurr = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(reinterpret_cast<BYTE*>(pCurr) + pCurr->NextEntryOffset);
        }

        for (auto it = processes.begin(); it != processes.end();) {
            if (it->second.seenGeneration != currentGeneration) {
                it = processes.erase(it);
            } else {
                ++it;
            }
        }

        {
            std::lock_guard<std::mutex> lock(g_UserCacheMutex);
            for (auto it = g_UserCache.begin(); it != g_UserCache.end();) {
                if (processes.find(it->first) == processes.end()) {
                    it = g_UserCache.erase(it);
                } else {
                    ++it;
                }
            }
        }

        totalTasks = tasks;
        totalThreads = threads;
        runningTasks = running;
        lastUpdateTime = now;
    }
};

