#include "htop_telemetry.hpp"

#include "htop_nt.hpp"

#include <algorithm>
#include <cstddef>
#include <mutex>
#include <psapi.h>
#include <vector>

struct CachedProcessOwner {
    ULONGLONG creationTime = 0;
    std::wstring username;
};

static std::unordered_map<DWORD, CachedProcessOwner> g_UserCache;
static std::mutex g_UserCacheMutex;

static void AppendTelemetryError(std::wstring& errors, const std::wstring& message) {
    if (!errors.empty()) errors += L"; ";
    errors += message;
}

static std::wstring GetProcessOwner(DWORD pid, ULONGLONG creationTime) {
    if (pid == 0 || pid == 4) return L"SYSTEM";

    {
        std::lock_guard<std::mutex> lock(g_UserCacheMutex);
        auto it = g_UserCache.find(pid);
        if (it != g_UserCache.end() && it->second.creationTime == creationTime) {
            return it->second.username;
        }
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return L"N/A";

    FILETIME processCreateTime, processExitTime, processKernelTime, processUserTime;
    if (!GetProcessTimes(hProcess, &processCreateTime, &processExitTime,
                         &processKernelTime, &processUserTime)) {
        CloseHandle(hProcess);
        return L"N/A";
    }

    ULONGLONG observedCreationTime =
        (static_cast<ULONGLONG>(processCreateTime.dwHighDateTime) << 32) |
        processCreateTime.dwLowDateTime;
    if (observedCreationTime != creationTime) {
        CloseHandle(hProcess);
        return L"N/A";
    }

    HANDLE hToken = NULL;
    std::wstring username = L"N/A";

    if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
        DWORD dwSize = 0;
        GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);
        if (dwSize > 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            std::vector<BYTE> buffer(dwSize);
            if (GetTokenInformation(hToken, TokenUser, buffer.data(), dwSize, &dwSize)) {
                PTOKEN_USER pTokenUser = reinterpret_cast<PTOKEN_USER>(buffer.data());
                DWORD nameLen = 0, domainLen = 0;
                SID_NAME_USE snu;
                LookupAccountSidW(NULL, pTokenUser->User.Sid, NULL, &nameLen,
                                  NULL, &domainLen, &snu);
                if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && nameLen > 0) {
                    std::vector<wchar_t> name(nameLen);
                    std::vector<wchar_t> domain(domainLen);
                    if (LookupAccountSidW(NULL, pTokenUser->User.Sid, name.data(), &nameLen,
                                          domain.empty() ? NULL : domain.data(), &domainLen, &snu)) {
                        username = domain.empty()
                            ? std::wstring(name.data())
                            : std::wstring(domain.data()) + L"\\" + name.data();
                    }
                }
            }
        }
        CloseHandle(hToken);
    }
    CloseHandle(hProcess);

    {
        std::lock_guard<std::mutex> lock(g_UserCacheMutex);
        g_UserCache[pid] = {creationTime, username};
    }

    return username;
}

void TelemetryEngine::Initialize() {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        DWORD processorCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        if (processorCount == 0) processorCount = sysInfo.dwNumberOfProcessors;
        cores.resize((std::max)(static_cast<DWORD>(1), processorCount));
        
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        if (GlobalMemoryStatusEx(&memStatus)) {
            totalRam = memStatus.ullTotalPhys;
        }

        UpdateSystemMetrics();
    }

void TelemetryEngine::UpdateSystemMetrics() {
        lastError.clear();
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

        // 2. Physical memory and system commit metrics
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        if (GlobalMemoryStatusEx(&memStatus)) {
            totalRam = memStatus.ullTotalPhys;
            availRam = memStatus.ullAvailPhys;
            usedRam = (totalRam >= availRam) ? totalRam - availRam : 0;
        } else {
            AppendTelemetryError(lastError, L"Physical memory telemetry query failed");
        }

        PERFORMANCE_INFORMATION perfInfo{};
        if (GetPerformanceInfo(&perfInfo, sizeof(perfInfo))) {
            commitLimit = static_cast<SIZE_T>(perfInfo.CommitLimit) * perfInfo.PageSize;
            commitTotal = static_cast<SIZE_T>(perfInfo.CommitTotal) * perfInfo.PageSize;
        } else {
            AppendTelemetryError(lastError, L"System commit telemetry query failed");
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

        size_t requiredPerfBytes =
            sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * cores.size();
        if (NT_SUCCESS(status) && returnLen >= requiredPerfBytes) {
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
        } else if (!NT_SUCCESS(status)) {
            AppendTelemetryError(lastError, L"CPU telemetry query failed");
        } else {
            AppendTelemetryError(lastError, L"CPU telemetry returned an invalid buffer length");
        }

        // 4. Processes & Threads via NtQuerySystemInformation
        constexpr ULONG maxProcBufferLen = 256 * 1024 * 1024;
        ULONG procBufferLen = 1024 * 1024;
        std::vector<BYTE> procBuffer(procBufferLen);
        NTSTATUS processStatus;
        for (;;) {
            returnLen = 0;
            processStatus = NtQuerySystemInformation(
                SystemProcessInformation, procBuffer.data(), procBufferLen, &returnLen);
            if (processStatus != static_cast<NTSTATUS>(0xC0000004)) break;
            if (procBufferLen >= maxProcBufferLen) {
                AppendTelemetryError(lastError, L"Process telemetry exceeded the 256 MiB safety limit");
                return;
            }
            ULONG nextLength = procBufferLen * 2;
            if (returnLen > nextLength) nextLength = returnLen;
            procBufferLen = (std::min)(nextLength, maxProcBufferLen);
            procBuffer.resize(procBufferLen);
        }
        if (!NT_SUCCESS(processStatus)) {
            wchar_t errorText[80];
            swprintf_s(errorText, L"Process telemetry query failed (NTSTATUS 0x%08lX)",
                       static_cast<unsigned long>(processStatus));
            AppendTelemetryError(lastError, errorText);
            return;
        }
        if (returnLen == 0 || returnLen > procBufferLen) {
            AppendTelemetryError(lastError, L"Process telemetry returned an invalid buffer length");
            return;
        }

        const size_t headerSize = offsetof(SYSTEM_PROCESS_INFORMATION, Threads);
        size_t validationOffset = 0;
        bool foundLastEntry = false;
        while (validationOffset < returnLen) {
            size_t remaining = returnLen - validationOffset;
            if (remaining < headerSize) {
                AppendTelemetryError(lastError, L"Process telemetry contained a truncated process entry");
                return;
            }

            auto* entry = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(
                procBuffer.data() + validationOffset);
            size_t entryLength = entry->NextEntryOffset == 0 ? remaining : entry->NextEntryOffset;
            if (entryLength < headerSize || entryLength > remaining ||
                entry->NumberOfThreads >
                    (entryLength - headerSize) / sizeof(SYSTEM_THREAD_INFORMATION)) {
                AppendTelemetryError(lastError, L"Process telemetry contained an invalid process entry");
                return;
            }

            const UNICODE_STRING& imageName = entry->ImageName;
            if (imageName.Length > imageName.MaximumLength ||
                imageName.Length % sizeof(wchar_t) != 0 ||
                (imageName.Length > 0 && imageName.Buffer == nullptr)) {
                AppendTelemetryError(lastError, L"Process telemetry contained an invalid image name");
                return;
            }
            if (imageName.Length > 0) {
                ULONG_PTR bufferStart = reinterpret_cast<ULONG_PTR>(procBuffer.data());
                ULONG_PTR imageStart = reinterpret_cast<ULONG_PTR>(imageName.Buffer);
                if (imageStart < bufferStart || imageStart - bufferStart > returnLen ||
                    imageName.Length > returnLen - (imageStart - bufferStart)) {
                    AppendTelemetryError(lastError, L"Process telemetry image name exceeded its buffer");
                    return;
                }
            }

            if (entry->NextEntryOffset == 0) {
                foundLastEntry = true;
                break;
            }
            validationOffset += entry->NextEntryOffset;
        }
        if (!foundLastEntry) {
            AppendTelemetryError(lastError, L"Process telemetry did not contain a complete process list");
            return;
        }

        ULONG tasks = 0, threads = 0, running = 0;
        ++currentGeneration;

        size_t offset = 0;
        while (offset <= returnLen && returnLen - offset >= headerSize) {
            auto* pCurr = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(procBuffer.data() + offset);
            size_t remaining = returnLen - offset;
            size_t entryLength = pCurr->NextEntryOffset == 0 ? remaining : pCurr->NextEntryOffset;
            if (entryLength < headerSize || entryLength > remaining ||
                pCurr->NumberOfThreads > (entryLength - headerSize) / sizeof(SYSTEM_THREAD_INFORMATION)) {
                AppendTelemetryError(lastError, L"Process telemetry contained an invalid process entry");
                return;
            }

            DWORD pid = HandleToUlong(pCurr->UniqueProcessId);
            DWORD ppid = HandleToUlong(pCurr->InheritedFromUniqueProcessId);

            auto [itProc, inserted] = processes.try_emplace(pid);
            ProcessItem& item = itProc->second;
            bool sameProcess = !inserted && item.creationTime ==
                               static_cast<ULONGLONG>(pCurr->CreateTime.QuadPart);
            bool tagged = sameProcess && item.isTagged;
            ULONGLONG prevTotalTime = item.totalCpuTime;

            item.pid = pid;
            item.ppid = ppid;
            item.isTagged = tagged;
            item.creationTime = static_cast<ULONGLONG>(pCurr->CreateTime.QuadPart);
            
            if (pCurr->ImageName.Buffer != nullptr && pCurr->ImageName.Length > 0) {
                item.name = std::wstring(pCurr->ImageName.Buffer, pCurr->ImageName.Length / sizeof(wchar_t));
            } else {
                item.name = (pid == 0) ? L"System Idle Process" : ((pid == 4) ? L"System" : L"Unknown");
            }

            item.user = GetProcessOwner(pid, item.creationTime);
            item.priority = pCurr->BasePriority;
            item.virtMem = pCurr->VirtualSize;
            item.resMem = pCurr->WorkingSetSize;
            const SIZE_T privateWorkingSet = pCurr->WorkingSetPrivateSize.QuadPart > 0
                ? static_cast<SIZE_T>(pCurr->WorkingSetPrivateSize.QuadPart)
                : 0;
            item.sharedMem = pCurr->WorkingSetSize >= privateWorkingSet
                ? pCurr->WorkingSetSize - privateWorkingSet
                : 0;
            item.threadCount = pCurr->NumberOfThreads;

            ULONGLONG totalTime = pCurr->KernelTime.QuadPart + pCurr->UserTime.QuadPart;
            item.totalCpuTime = totalTime;

            if (elapsed100ns > 0.0 && sameProcess && !cores.empty()) {
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
            offset += pCurr->NextEntryOffset;
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
