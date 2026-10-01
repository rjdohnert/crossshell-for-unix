/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, cmd-extended contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * HTOP (Native Win Clone)
 *
 * Single File Index
 * - AppInfo, NT native declarations, and OS integration helpers
 * - Color/theme constants and formatting helpers
 * - TelemetryEngine: process, CPU, memory, and owner sampling
 * - ConsoleRenderer: VT output, buffers, and screen management
 * - HtopApp: state machine, input handling, rendering, and actions
 * - main(): startup, version handling, and application launch
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <sddl.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <atomic>
#include <cwctype>
#include <io.h>
#include <fcntl.h>
#include <conio.h>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "ntdll.lib")

namespace AppInfo {
    constexpr wchar_t NAME[] = L"htop";
    constexpr wchar_t VERSION[] = L"5.9";
}

// ============================================================================
// NTDLL NATIVE APIS & STRUCTURES
// ============================================================================
#define SystemProcessorPerformanceInformation 8
#define SystemProcessInformation 5

typedef LONG NTSTATUS;
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING;

typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION {
    LARGE_INTEGER IdleTime;
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER DpcTime;
    LARGE_INTEGER InterruptTime;
    ULONG InterruptCount;
} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;

typedef struct _SYSTEM_THREAD_INFORMATION {
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER CreateTime;
    ULONG WaitTime;
    PVOID StartAddress;
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
    LONG Priority;
    LONG BasePriority;
    ULONG ContextSwitches;
    ULONG ThreadState;
    ULONG WaitReason;
} SYSTEM_THREAD_INFORMATION;

typedef struct _SYSTEM_PROCESS_INFORMATION {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER WorkingSetPrivateSize;
    ULONG HardFaultCount;
    ULONG NumberOfThreadsHighWatermark;
    ULONGLONG CycleTime;
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    LONG BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR UniqueProcessKey;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER ReadOperationCount;
    LARGE_INTEGER WriteOperationCount;
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
    SYSTEM_THREAD_INFORMATION Threads[1];
} SYSTEM_PROCESS_INFORMATION;

extern "C" NTSYSAPI NTSTATUS NTAPI NtQuerySystemInformation(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

extern "C" NTSYSAPI NTSTATUS NTAPI NtSuspendProcess(HANDLE ProcessHandle);
extern "C" NTSYSAPI NTSTATUS NTAPI NtResumeProcess(HANDLE ProcessHandle);

static bool InitializeNtApis() {
    // Linked via ntdll.lib; no runtime symbol probing needed.
    return true;
}

// ============================================================================
// ANSI COLOR CODES & THEME ENGINE
// ============================================================================
namespace Color {
    constexpr wchar_t RESET[]            = L"\033[0m";
    constexpr wchar_t BOLD[]             = L"\033[1m";
    constexpr wchar_t DIM[]              = L"\033[2m";
    
    // Foreground
    constexpr wchar_t BLACK[]            = L"\033[30m";
    constexpr wchar_t RED[]              = L"\033[31m";
    constexpr wchar_t GREEN[]            = L"\033[32m";
    constexpr wchar_t YELLOW[]           = L"\033[33m";
    constexpr wchar_t BLUE[]             = L"\033[34m";
    constexpr wchar_t MAGENTA[]          = L"\033[35m";
    constexpr wchar_t CYAN[]             = L"\033[36m";
    constexpr wchar_t WHITE[]            = L"\033[37m";
    
    constexpr wchar_t BRIGHT_GREEN[]     = L"\033[92m";
    constexpr wchar_t BRIGHT_YELLOW[]    = L"\033[93m";
    constexpr wchar_t BRIGHT_CYAN[]      = L"\033[96m";
    constexpr wchar_t BRIGHT_WHITE[]     = L"\033[97m";

    // Background
    constexpr wchar_t BG_DEFAULT[]       = L"\033[49m";
    constexpr wchar_t BG_RED[]           = L"\033[41m";
    constexpr wchar_t BG_BLUE[]          = L"\033[44m";
    constexpr wchar_t BG_CYAN[]          = L"\033[46m";
    constexpr wchar_t BG_GRAY[]          = L"\033[100m";
    constexpr wchar_t BG_SELECTION[]     = L"\033[44;37;1m"; // White on Blue
    constexpr wchar_t BG_TAGGED[]        = L"\033[43;30;1m"; // Black on Yellow
    constexpr wchar_t BG_HEADER[]        = L"\033[42;37;1m"; // White on Green
    constexpr wchar_t BG_FOOTER[]        = L"\033[40;37m";
}

// ============================================================================
// DATA MODELS & ENUMS
// ============================================================================
enum class SortField {
    PID,
    USER,
    PRIORITY,
    VIRTUAL_MEM,
    RESIDENT_MEM,
    SHARED_MEM,
    STATE,
    CPU_PERCENT,
    MEM_PERCENT,
    TIME,
    COMMAND
};

enum class ViewMode {
    Normal,
    HelpModal,
    SetupModal,
    SearchInput,
    FilterInput,
    SignalModal,
    UserFilterInput
};

struct CpuCoreData {
    ULONGLONG idleTime = 0;
    ULONGLONG kernelTime = 0;
    ULONGLONG userTime = 0;
    double usagePercent = 0.0;
};

struct ProcessItem {
    DWORD pid = 0;
    DWORD ppid = 0;
    std::wstring name;
    std::wstring fullPath;
    std::wstring user;
    LONG priority = 0;
    SIZE_T virtMem = 0;
    SIZE_T resMem = 0;
    SIZE_T privMem = 0;
    wchar_t state = L'R';
    double cpuPercent = 0.0;
    double memPercent = 0.0;
    ULONGLONG totalCpuTime = 0; // In 100-ns units
    DWORD threadCount = 0;
    bool isTagged = false;
    
    // Parent-child tree structure
    std::vector<DWORD> childPids;
    size_t treeDepth = 0;
    std::wstring treePrefix;
    uint64_t seenGeneration = 0;
};

// User SID Cache to optimize token inspection
static std::unordered_map<DWORD, std::wstring> g_UserCache;
static std::mutex g_UserCacheMutex;

static std::wstring GetProcessOwner(DWORD pid) {
    if (pid == 0 || pid == 4) return L"SYSTEM";

    {
        std::lock_guard<std::mutex> lock(g_UserCacheMutex);
        auto it = g_UserCache.find(pid);
        if (it != g_UserCache.end()) return it->second;
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return L"N/A";

    HANDLE hToken = NULL;
    std::wstring username = L"N/A";

    if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
        DWORD dwSize = 0;
        GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);
        if (dwSize > 0) {
            std::vector<BYTE> buffer(dwSize);
            if (GetTokenInformation(hToken, TokenUser, buffer.data(), dwSize, &dwSize)) {
                PTOKEN_USER pTokenUser = reinterpret_cast<PTOKEN_USER>(buffer.data());
                wchar_t name[256], domain[256];
                DWORD nameLen = 256, domainLen = 256;
                SID_NAME_USE snu;
                if (LookupAccountSidW(NULL, pTokenUser->User.Sid, name, &nameLen, domain, &domainLen, &snu)) {
                    username = std::wstring(domain) + L"\\" + std::wstring(name);
                }
            }
        }
        CloseHandle(hToken);
    }
    CloseHandle(hProcess);

    {
        std::lock_guard<std::mutex> lock(g_UserCacheMutex);
        g_UserCache[pid] = username;
    }

    return username;
}

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

// ============================================================================
// FORMATTING HELPERS
// ============================================================================
static std::wstring FormatBytes(SIZE_T bytes) {
    double db = static_cast<double>(bytes);
    wchar_t buf[32];
    if (db >= 1024.0 * 1024.0 * 1024.0) {
        swprintf_s(buf, L"%.2fG", db / (1024.0 * 1024.0 * 1024.0));
    } else if (db >= 1024.0 * 1024.0) {
        swprintf_s(buf, L"%.1fM", db / (1024.0 * 1024.0));
    } else if (db >= 1024.0) {
        swprintf_s(buf, L"%.0fK", db / 1024.0);
    } else {
        swprintf_s(buf, L"%zudB", bytes);
    }
    return buf;
}

static std::wstring FormatCpuTime(ULONGLONG time100ns) {
    ULONGLONG totalSeconds = time100ns / 10000000ULL;
    ULONGLONG hours = totalSeconds / 3600;
    ULONGLONG minutes = (totalSeconds % 3600) / 60;
    ULONGLONG seconds = totalSeconds % 60;
    ULONGLONG hundredths = (time100ns / 100000ULL) % 100;

    wchar_t buf[32];
    if (hours > 0) {
        swprintf_s(buf, L"%lluh%02llu:%02llu", hours, minutes, seconds);
    } else {
        swprintf_s(buf, L"%02llu:%02llu.%02llu", minutes, seconds, hundredths);
    }
    return buf;
}

static std::wstring FormatUptime(ULONGLONG seconds) {
    ULONGLONG days = seconds / 86400;
    ULONGLONG hours = (seconds % 86400) / 3600;
    ULONGLONG minutes = (seconds % 3600) / 60;
    ULONGLONG secs = seconds % 60;

    wchar_t buf[64];
    if (days > 0) {
        swprintf_s(buf, L"%llud %02llu:%02llu:%02llu", days, hours, minutes, secs);
    } else {
        swprintf_s(buf, L"%02llu:%02llu:%02llu", hours, minutes, secs);
    }
    return buf;
}

// ============================================================================
// CONSOLE & TERMINAL RENDER ENGINE (Double-Buffered VT100)
// ============================================================================
class ConsoleRenderer {
private:
    HANDLE m_hOut;
    HANDLE m_hIn;
    DWORD m_origOutMode;
    DWORD m_origInMode;
    SHORT m_width = 80;
    SHORT m_height = 25;
    bool m_initialized = false;
    bool m_isConsoleInput = false;

    std::wstring m_buffer;

public:
    ConsoleRenderer() : m_hOut(INVALID_HANDLE_VALUE), m_hIn(INVALID_HANDLE_VALUE) {}

    bool Initialize() {
        m_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        m_hIn = GetStdHandle(STD_INPUT_HANDLE);

        if (m_hOut == INVALID_HANDLE_VALUE || m_hIn == INVALID_HANDLE_VALUE) return false;

        if (!GetConsoleMode(m_hOut, &m_origOutMode)) return false;
        if (!GetConsoleMode(m_hIn, &m_origInMode)) return false;
        m_isConsoleInput = true;

        // Enable VT100 escapes & Mouse Input. If VT cannot be enabled, abort to avoid garbled output.
        DWORD newOutMode = m_origOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        DWORD newInMode = (m_origInMode | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS) & ~ENABLE_QUICK_EDIT_MODE;

        if (!SetConsoleMode(m_hOut, newOutMode)) return false;
        if (!SetConsoleMode(m_hIn, newInMode)) return false;

        _setmode(_fileno(stdout), _O_U16TEXT);

        // Switch to alternate screen and hide cursor for full-screen TUI behavior.
        std::wcout << L"\033[?1049h\033[?25l\033[H\033[?1000h\033[?1006h";
        UpdateDimensions();
        m_initialized = true;
        return true;
    }

    ~ConsoleRenderer() {
        if (!m_initialized) return;

        // Restore screen/cursor and console modes.
        std::wcout << Color::RESET << L"\033[?1000l\033[?1006l\033[?25h\033[?1049l";
        SetConsoleMode(m_hOut, m_origOutMode);
        SetConsoleMode(m_hIn, m_origInMode);
    }

    void UpdateDimensions() {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(m_hOut, &csbi)) {
            m_width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            m_height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        }
    }

    SHORT GetWidth() const { return m_width; }
    SHORT GetHeight() const { return m_height; }
    bool IsConsoleInput() const { return m_isConsoleInput; }

    void ClearBuffer() {
        m_buffer.clear();
        // Keep a reusable frame buffer to reduce allocations in the hot render loop.
        size_t targetChars = static_cast<size_t>((std::max<SHORT>)(m_width, 80))
                           * static_cast<size_t>((std::max<SHORT>)(m_height, 25))
                           * 2;
        if (m_buffer.capacity() < targetChars) {
            m_buffer.reserve(targetChars);
        }
        m_buffer += L"\033[2J\033[H"; // Clear screen and move cursor to top-left
    }

    void Write(const std::wstring& str) {
        m_buffer += str;
    }

    void Flush() {
        if (m_buffer.empty()) return;
        DWORD written = 0;
        WriteConsoleW(m_hOut, m_buffer.c_str(), static_cast<DWORD>(m_buffer.length()), &written, NULL);
    }

    std::wstring DrawBar(int width, double percent, const wchar_t* color) {
        int fill = static_cast<int>((percent / 100.0) * width);
        fill = (std::max)(0, (std::min)(width, fill));

        std::wstring bar;
        bar.reserve(static_cast<size_t>(width) + 16);
        bar += L"[";
        bar += color;
        bar.append(static_cast<size_t>(fill), L'|');
        bar += Color::RESET;
        bar.append(static_cast<size_t>(width - fill), L' ');
        bar += L"]";
        return bar;
    }
};

// ============================================================================
// MAIN APPLICATION STATE MACHINE
// ============================================================================
class HtopApp {
private:
    TelemetryEngine m_telemetry;
    ConsoleRenderer m_renderer;

    ViewMode m_currentMode = ViewMode::Normal;
    SortField m_sortField = SortField::CPU_PERCENT;
    bool m_sortAscending = false;
    bool m_treeView = false;
    bool m_showThreads = false;
    bool m_useUnicodeTreeGraphics = false;
    bool m_highlightProgramBasename = false;
    bool m_autoRefreshTelemetry = true;

    size_t m_setupSelectedOption = 0;
    static constexpr size_t kSetupOptionCount = 4;

    size_t m_selectedIndex = 0;
    size_t m_scrollOffset = 0;

    std::wstring m_searchQuery;
    std::wstring m_filterQuery;
    std::wstring m_userFilterQuery;
    bool m_searchHasMatch = true;
    size_t m_searchMatchIndex = 0;
    size_t m_searchMatchTotal = 0;

    std::vector<DWORD> m_displayPids;
    bool m_isRunning = true;
    int m_lastHeaderLines = 0;

    static std::wstring TruncateForCell(const std::wstring& text, int maxChars) {
        if (maxChars <= 0) return L"";
        if (static_cast<int>(text.size()) <= maxChars) return text;
        if (maxChars <= 1) return text.substr(0, 1);
        return text.substr(0, maxChars - 1) + L"~";
    }

    static std::wstring ToLowerCopy(const std::wstring& value) {
        std::wstring lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
            [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return lowered;
    }

    template <typename... Args>
    static void AppendFormat(std::wstring& out, const wchar_t* format, Args... args) {
        wchar_t buffer[512];
        swprintf_s(buffer, format, args...);
        out += buffer;
    }

    static void AppendRepeated(std::wstring& out, wchar_t ch, int count) {
        if (count > 0) {
            out.append(static_cast<size_t>(count), ch);
        }
    }

    bool ProcessMatchesNeedle(const ProcessItem& proc, const std::wstring& needleLower) const {
        std::wstring candidate = proc.name + L" " + proc.user + L" " + std::to_wstring(proc.pid);
        return ToLowerCopy(candidate).find(needleLower) != std::wstring::npos;
    }

    void RefreshSearchMatchStats(const std::wstring& needleLower, size_t selectedMatchOrdinal = static_cast<size_t>(-1)) {
        m_searchMatchTotal = 0;
        m_searchMatchIndex = 0;

        if (needleLower.empty()) {
            m_searchHasMatch = true;
            return;
        }

        for (size_t i = 0; i < m_displayPids.size(); ++i) {
            DWORD pid = m_displayPids[i];
            const auto& proc = m_telemetry.processes[pid];
            if (!ProcessMatchesNeedle(proc, needleLower)) continue;

            m_searchMatchTotal++;
            if (i == m_selectedIndex) {
                m_searchMatchIndex = m_searchMatchTotal;
            }
        }

        if (selectedMatchOrdinal != static_cast<size_t>(-1) && selectedMatchOrdinal > 0) {
            m_searchMatchIndex = selectedMatchOrdinal;
        }

        m_searchHasMatch = (m_searchMatchTotal > 0);
    }

public:
    void Run() {
        if (!m_renderer.Initialize()) {
            std::wcerr << L"Error: Failed to initialize Win32 console environment.\n";
            return;
        }

        m_telemetry.Initialize();

        // Application Loop
        while (m_isRunning) {
            m_renderer.UpdateDimensions();
            if (m_autoRefreshTelemetry) {
                m_telemetry.UpdateSystemMetrics();
            }

            BuildProcessDisplayList();
            Render();

            // Non-blocking Input Loop during tick delay
            auto startTick = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now() - startTick < std::chrono::milliseconds(1500)) {
                bool inputChanged = ProcessInput();
                if (inputChanged && m_isRunning) {
                    // Render immediately on interaction for responsive key/mouse feedback.
                    BuildProcessDisplayList();
                    Render();
                }
                if (!m_isRunning) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
            }
        }
    }

private:
    void BuildProcessDisplayList() {
        m_displayPids.clear();

        if (m_treeView) {
            // Build Parent-Child Graph
            std::unordered_map<DWORD, std::vector<DWORD>> treeMap;
            std::unordered_set<DWORD> allPids;

            for (const auto& [pid, proc] : m_telemetry.processes) {
                allPids.insert(pid);
                treeMap[proc.ppid].push_back(pid);
            }

            // Find Roots
            std::vector<DWORD> roots;
            for (const auto& [pid, proc] : m_telemetry.processes) {
                if (proc.ppid == 0 || proc.ppid == pid || allPids.find(proc.ppid) == allPids.end()) {
                    roots.push_back(pid);
                }
            }

            std::unordered_set<DWORD> visited;
            auto FlattenTree = [&](auto self, DWORD pid, size_t depth, const std::wstring& prefix) -> void {
                if (visited.count(pid)) return;
                visited.insert(pid);

                auto& proc = m_telemetry.processes[pid];
                proc.treeDepth = depth;
                proc.treePrefix = prefix;

                // Apply Filters
                if (MatchesFilter(proc)) {
                    m_displayPids.push_back(pid);
                }

                const auto& children = treeMap[pid];
                for (size_t i = 0; i < children.size(); ++i) {
                    bool isLast = (i == children.size() - 1);
                    std::wstring childPrefix;
                    std::wstring passPrefix;
                    if (m_useUnicodeTreeGraphics) {
                        childPrefix = prefix + (isLast ? L"└─ " : L"├─ ");
                        passPrefix = prefix + (isLast ? L"   " : L"│  ");
                    } else {
                        // ASCII connectors avoid garbled output in terminals with limited glyph support.
                        childPrefix = prefix + (isLast ? L"`-- " : L"|-- ");
                        passPrefix = prefix + (isLast ? L"    " : L"|   ");
                    }
                    self(self, children[i], depth + 1, childPrefix);
                }
            };

            for (DWORD rootPid : roots) {
                FlattenTree(FlattenTree, rootPid, 0, L"");
            }
        } else {
            // Flat List
            for (const auto& [pid, proc] : m_telemetry.processes) {
                if (MatchesFilter(proc)) {
                    m_displayPids.push_back(pid);
                }
            }

            // Sort Flat List
            std::sort(m_displayPids.begin(), m_displayPids.end(), [this](DWORD a, DWORD b) {
                const auto& pA = m_telemetry.processes[a];
                const auto& pB = m_telemetry.processes[b];

                bool result = false;
                switch (m_sortField) {
                    case SortField::PID: result = pA.pid < pB.pid; break;
                    case SortField::USER: result = pA.user < pB.user; break;
                    case SortField::PRIORITY: result = pA.priority < pB.priority; break;
                    case SortField::VIRTUAL_MEM: result = pA.virtMem < pB.virtMem; break;
                    case SortField::RESIDENT_MEM: result = pA.resMem < pB.resMem; break;
                    case SortField::SHARED_MEM: result = pA.privMem < pB.privMem; break;
                    case SortField::STATE: result = pA.state < pB.state; break;
                    case SortField::CPU_PERCENT: result = pA.cpuPercent < pB.cpuPercent; break;
                    case SortField::MEM_PERCENT: result = pA.memPercent < pB.memPercent; break;
                    case SortField::TIME: result = pA.totalCpuTime < pB.totalCpuTime; break;
                    case SortField::COMMAND: result = pA.name < pB.name; break;
                }
                return m_sortAscending ? result : !result;
            });
        }

        // Clamp selection index bounds
        if (m_displayPids.empty()) {
            m_selectedIndex = 0;
            m_scrollOffset = 0;
        } else {
            if (m_selectedIndex >= m_displayPids.size()) {
                m_selectedIndex = m_displayPids.size() - 1;
            }
        }
    }

    bool MatchesFilter(const ProcessItem& proc) {
        if (!m_filterQuery.empty()) {
            if (proc.name.find(m_filterQuery) == std::wstring::npos &&
                std::to_wstring(proc.pid).find(m_filterQuery) == std::wstring::npos) {
                return false;
            }
        }

        if (!m_userFilterQuery.empty()) {
            if (proc.user.find(m_userFilterQuery) == std::wstring::npos) {
                return false;
            }
        }

        return true;
    }

    // ========================================================================
    // RENDERING PIPELINE
    // ========================================================================
    void Render() {
        m_renderer.ClearBuffer();

        SHORT width = m_renderer.GetWidth();
        SHORT height = m_renderer.GetHeight();

        if (width < 80 || height < 20) {
            std::wstring msg;
            msg.reserve(128);
            msg += Color::BOLD;
            msg += AppInfo::NAME;
            msg += L" ";
            msg += AppInfo::VERSION;
            msg += Color::RESET;
            msg += L"\nTerminal too small for TUI rendering. Resize to at least 80x20.\nCurrent size: ";
            AppendFormat(msg, L"%hdx%hd\n", width, height);
            m_renderer.Write(msg);
            m_renderer.Flush();
            return;
        }

        int headerLines = RenderHeader(width);
        m_lastHeaderLines = headerLines;
        int footerLines = 2; // Summary bar + F-keys
        int processLinesAvailable = height - headerLines - footerLines;

        if (processLinesAvailable > 0) {
            RenderTableHeader(width);
            RenderProcessTable(processLinesAvailable - 1, width);
        }

        RenderFooter(width, height);

        // Render Overlays / Modals
        if (m_currentMode == ViewMode::HelpModal) RenderHelpModal(width, height);
        if (m_currentMode == ViewMode::SetupModal) RenderSetupModal(width, height);
        if (m_currentMode == ViewMode::SignalModal) RenderSignalModal(width, height);

        m_renderer.Flush();
    }

    int RenderHeader(SHORT width) {
        int linesDrawn = 0;
        int halfWidth = width / 2 - 2;

        size_t numCores = m_telemetry.cores.size();
        int coreColumns = (numCores > 8) ? 2 : 1;
        int rowsPerColumn = (coreColumns == 2) ? static_cast<int>((numCores + 1) / 2) : static_cast<int>(numCores);

        // Limit CPU core rows if terminal is tiny
        rowsPerColumn = (std::min)(rowsPerColumn, 8);

        for (int r = 0; r < rowsPerColumn; ++r) {
            std::wstring line;
            line.reserve(256);

            // Column 1
            if (r < static_cast<int>(numCores)) {
                wchar_t coreBuf[16];
                swprintf_s(coreBuf, L"%-2d", r + 1);
                line += Color::CYAN;
                line += coreBuf;
                line += Color::RESET;
                line += L" ";
                line += m_renderer.DrawBar((std::max)(10, halfWidth - 12), m_telemetry.cores[r].usagePercent, Color::GREEN);
                
                wchar_t pctBuf[16];
                swprintf_s(pctBuf, L"%5.1f%%", m_telemetry.cores[r].usagePercent);
                line += Color::BOLD;
                line += pctBuf;
                line += Color::RESET;
            }

            // Column 2 (Right side telemetry meters)
            line += L"  ";
            if (coreColumns == 2 && (r + rowsPerColumn) < static_cast<int>(numCores)) {
                int c2 = r + rowsPerColumn;
                wchar_t coreBuf[16];
                swprintf_s(coreBuf, L"%-2d", c2 + 1);
                line += Color::CYAN;
                line += coreBuf;
                line += Color::RESET;
                line += L" ";
                line += m_renderer.DrawBar((std::max)(10, halfWidth - 12), m_telemetry.cores[c2].usagePercent, Color::GREEN);
                
                wchar_t pctBuf[16];
                swprintf_s(pctBuf, L"%5.1f%%", m_telemetry.cores[c2].usagePercent);
                line += Color::BOLD;
                line += pctBuf;
                line += Color::RESET;
            } else if (r == 0) {
                // Memory Bar
                line += Color::CYAN;
                line += L"Mem ";
                line += Color::RESET;
                double memPct = (m_telemetry.totalRam > 0) ? ((double)m_telemetry.usedRam / m_telemetry.totalRam) * 100.0 : 0.0;
                line += m_renderer.DrawBar((std::max)(10, halfWidth - 22), memPct, Color::GREEN);
                line += L" ";
                line += FormatBytes(m_telemetry.usedRam);
                line += L"/";
                line += FormatBytes(m_telemetry.totalRam);
            } else if (r == 1) {
                // Pagefile / Swap Bar
                line += Color::CYAN;
                line += L"Swp ";
                line += Color::RESET;
                double swpPct = (m_telemetry.totalPagefile > 0) ? ((double)m_telemetry.usedPagefile / m_telemetry.totalPagefile) * 100.0 : 0.0;
                line += m_renderer.DrawBar((std::max)(10, halfWidth - 22), swpPct, Color::GREEN);
                line += L" ";
                line += FormatBytes(m_telemetry.usedPagefile);
                line += L"/";
                line += FormatBytes(m_telemetry.totalPagefile);
            } else if (r == 2) {
                // Tasks Count
                line += Color::BOLD;
                line += L"Tasks: ";
                line += Color::RESET;
                AppendFormat(line, L"%lu, %lu thr; ", m_telemetry.totalTasks, m_telemetry.totalThreads);
                line += Color::GREEN;
                AppendFormat(line, L"%lu running", m_telemetry.runningTasks);
                line += Color::RESET;
            } else if (r == 3) {
                // Load Average / Queue Length & Uptime
                line += Color::BOLD;
                line += L"Load average: ";
                line += Color::RESET;
                AppendFormat(line, L"%lu.00 ", m_telemetry.processorQueueLength);
                line += Color::BOLD;
                line += L"Uptime: ";
                line += Color::RESET;
                line += FormatUptime(m_telemetry.uptimeSeconds);
            }

            line += L"\n";
            m_renderer.Write(line);
            linesDrawn++;
        }

        return linesDrawn;
    }

    void RenderTableHeader(SHORT width) {
        std::wstring line;
        line.reserve(256);
        line += Color::BG_HEADER;

        wchar_t buf[256];
        swprintf_s(buf, L" %8s %-20s %5s %8s %8s %8s %3s %7s %7s %10s   ",
                  L"PID", L"USER", L"PRI", L"VIRT", L"RES", L"SHR", L"S", L"CPU%", L"MEM%", L"TIME+");
        line += buf;

        int usedLen = static_cast<int>(wcslen(buf));
        int remLen = (std::max)(0, width - usedLen);
        
        std::wstring cmdHeader = L"Command";
        if (remLen > static_cast<int>(cmdHeader.length())) {
            line += cmdHeader;
            AppendRepeated(line, L' ', remLen - static_cast<int>(cmdHeader.length()));
        }

        line += Color::RESET;
        line += L"\n";
        m_renderer.Write(line);
    }

    void RenderProcessTable(int availableLines, SHORT width) {
        if (m_displayPids.empty()) {
            m_renderer.Write(L"  [No matching processes]\n");
            return;
        }

        // Adjust scroll window bounds
        if (m_selectedIndex < m_scrollOffset) {
            m_scrollOffset = m_selectedIndex;
        } else if (m_selectedIndex >= m_scrollOffset + availableLines) {
            m_scrollOffset = m_selectedIndex - availableLines + 1;
        }

        for (int i = 0; i < availableLines; ++i) {
            size_t idx = m_scrollOffset + i;
            if (idx >= m_displayPids.size()) {
                m_renderer.Write(L"\n");
                continue;
            }

            DWORD pid = m_displayPids[idx];
            const auto& proc = m_telemetry.processes[pid];

            bool isSelected = (idx == m_selectedIndex);
            
            std::wstring line;
            line.reserve(256);
            if (proc.isTagged) {
                line += Color::BG_TAGGED;
            } else if (isSelected) {
                line += Color::BG_SELECTION;
            }

            wchar_t buf[256];
            swprintf_s(buf, L" %8lu %-20s %5ld %8s %8s %8s %3c %7.1f %7.1f %10s   ",
                      proc.pid,
                      proc.user.substr(0, 20).c_str(),
                      proc.priority,
                      FormatBytes(proc.virtMem).c_str(),
                      FormatBytes(proc.resMem).c_str(),
                      FormatBytes(proc.privMem).c_str(),
                      proc.state,
                      proc.cpuPercent,
                      proc.memPercent,
                      FormatCpuTime(proc.totalCpuTime).c_str());
            line += buf;

            // Command / Tree Column
            std::wstring cmdDisplay;
            if (m_treeView) {
                cmdDisplay = proc.treePrefix + proc.name;
            } else {
                cmdDisplay = proc.name;
            }

            if (m_showThreads) {
                cmdDisplay += L" {thr:" + std::to_wstring(proc.threadCount) + L"}";
            }

            std::transform(cmdDisplay.begin(), cmdDisplay.end(), cmdDisplay.begin(), [](wchar_t ch) {
                return static_cast<wchar_t>(std::towupper(ch));
            });

            int fixedCols = static_cast<int>(wcslen(buf));
            int cmdRoom = (std::max)(0, static_cast<int>(width) - fixedCols - 1);

            bool emphasizeCommand = m_highlightProgramBasename && !isSelected && !proc.isTagged;
            if (emphasizeCommand) {
                line += Color::BRIGHT_CYAN;
                line += Color::BOLD;
            }
            line += TruncateForCell(cmdDisplay, cmdRoom);
            if (emphasizeCommand) {
                line += Color::RESET;
            }

            if (isSelected || proc.isTagged) {
                line += Color::RESET;
            }

            line += L"\n";
            m_renderer.Write(line);
        }
    }

    void RenderFooter(SHORT width, SHORT height) {
        std::wstring line;
        line.reserve(256);

        // Line 1: Mode Filter status bar
        if (m_currentMode == ViewMode::SearchInput) {
            std::wstring payload = L" Search: " + m_searchQuery;
            if (!m_searchQuery.empty()) {
                if (m_searchHasMatch) {
                    payload += L"  [" + std::to_wstring(m_searchMatchIndex) + L"/" + std::to_wstring(m_searchMatchTotal) + L"]";
                } else {
                    payload += L"  [no matches]";
                }
            }
            line += Color::BG_CYAN;
            line += Color::BLACK;
            line += TruncateForCell(payload, (std::max)(0, static_cast<int>(width)));
            line += Color::RESET;
            line += L"\n";
        } else if (m_currentMode == ViewMode::FilterInput) {
            std::wstring payload = L" Filter: " + m_filterQuery;
            line += Color::BG_CYAN;
            line += Color::BLACK;
            line += TruncateForCell(payload, (std::max)(0, static_cast<int>(width)));
            line += Color::RESET;
            line += L"\n";
        } else if (m_currentMode == ViewMode::UserFilterInput) {
            std::wstring payload = L" User Filter: " + m_userFilterQuery;
            line += Color::BG_CYAN;
            line += Color::BLACK;
            line += TruncateForCell(payload, (std::max)(0, static_cast<int>(width)));
            line += Color::RESET;
            line += L"\n";
        } else {
            // Quick status info
            std::wstring status;
            status.reserve(128);
            status += AppInfo::NAME;
            status += L" ";
            status += AppInfo::VERSION;
            status += L" | Active Filter: ";
            status += (m_filterQuery.empty() ? L"[None]" : m_filterQuery);
            status += L" | Tagged: ";
            AppendFormat(status, L"%zu", CountTagged());
            line += Color::DIM;
            line += TruncateForCell(status, (std::max)(0, static_cast<int>(width)));
            line += Color::RESET;
            line += L"\n";
        }

        // Line 2: Function Keys Bar
        line += Color::BG_FOOTER;
        if (width >= 110) {
            line += Color::GREEN; line += L"F1"; line += Color::WHITE; line += L" Help  ";
            line += Color::GREEN; line += L"F2"; line += Color::WHITE; line += L" Setup  ";
            line += Color::GREEN; line += L"F3"; line += Color::WHITE; line += L" Search  ";
            line += Color::GREEN; line += L"F4"; line += Color::WHITE; line += L" Filter  ";
            line += Color::GREEN; line += L"F5"; line += Color::WHITE; line += (m_treeView ? L" Flat  " : L" Tree  ");
            line += Color::GREEN; line += L"F6"; line += Color::WHITE; line += L" Sort  ";
            line += Color::GREEN; line += L"F7"; line += Color::WHITE; line += L" Nice-  ";
            line += Color::GREEN; line += L"F8"; line += Color::WHITE; line += L" Nice+  ";
            line += Color::GREEN; line += L"F9"; line += Color::WHITE; line += L" Kill  ";
            line += Color::GREEN; line += L"F10"; line += Color::WHITE; line += L"Quit";
        } else {
            std::wstring compact = m_treeView
                ? L"F1 Help   F2 Setup   F3 Search   F4 Filter   F5 Flat   F10 Quit"
                : L"F1 Help   F2 Setup   F3 Search   F4 Filter   F5 Tree   F10 Quit";
            line += Color::WHITE;
            line += TruncateForCell(compact, (std::max)(0, static_cast<int>(width)));
        }

        line += Color::RESET;
        m_renderer.Write(line);
    }

    size_t CountTagged() {
        size_t count = 0;
        for (const auto& [pid, proc] : m_telemetry.processes) {
            if (proc.isTagged) count++;
        }
        return count;
    }

    // ========================================================================
    // MODAL DIALOG RENDERERS
    // ========================================================================
    void RenderHelpModal(SHORT width, SHORT height) {
        std::wstring line;
        line.reserve(1024);
        line += L"\033[3;10H";
        line += Color::BG_BLUE;
        line += Color::WHITE;
        line += Color::BOLD;
        line += L" Keybindings & Help Manual ";
        line += Color::RESET;
        
        const wchar_t* helpLines[] = {
            L" Arrow Keys / PgUp / PgDn : Navigate process table",
            L" Space                    : Tag / Untag selected process",
            L" U                        : Clear all process tags",
            L" F1 / h                   : Open Help Modal",
            L" F2 / S                   : Open Setup Modal",
            L" F3 / /                   : Incremental Search",
            L" F4 / \\                   : Dynamic Filter",
            L" F5 / t                   : Toggle Tree View / Flat View",
            L" F6 / < / >               : Change Sort Field",
            L" F7 / [                   : Lower Priority Class",
            L" F8 / ]                   : Raise Priority Class",
            L" F9 / k                   : Send Signal / Kill Process",
            L" u                        : Filter by User Name",
            L" Press ESC or F1 to close this manual."
        };

        for (size_t i = 0; i < 14; ++i) {
            wchar_t posBuf[32];
            swprintf_s(posBuf, L"\033[%zu;10H", 4 + i);
            line += posBuf;
            line += Color::BG_GRAY;
            line += Color::WHITE;
            line += helpLines[i];
            line += Color::RESET;
        }

        m_renderer.Write(line);
    }

    void RenderSetupModal(SHORT width, SHORT height) {
        std::wstring line;
        line.reserve(1024);
        line += L"\033[5;15H";
        line += Color::BG_CYAN;
        line += Color::BLACK;
        line += Color::BOLD;
        line += L" Setup & Configuration ";
        line += Color::RESET;

        std::wstring setupLines[kSetupOptionCount] = {
            std::wstring(L"[") + (m_showThreads ? L"X" : L" ") + L"] Display Threads in Process List",
            std::wstring(L"[") + (m_useUnicodeTreeGraphics ? L"X" : L" ") + L"] Enable UTF-16 Unicode Tree Graphics",
            std::wstring(L"[") + (m_highlightProgramBasename ? L"X" : L" ") + L"] Highlight Program Basename",
            std::wstring(L"[") + (m_autoRefreshTelemetry ? L"X" : L" ") + L"] Auto-refresh Telemetry Every 1.5s"
        };

        for (size_t i = 0; i < kSetupOptionCount; ++i) {
            wchar_t posBuf[32];
            swprintf_s(posBuf, L"\033[%zu;15H", 6 + i);
            line += posBuf;
            if (i == m_setupSelectedOption) {
                line += Color::BG_SELECTION;
                line += L" ";
                line += setupLines[i];
                line += L" ";
                line += Color::RESET;
            } else {
                line += Color::BG_GRAY;
                line += Color::WHITE;
                line += L" ";
                line += setupLines[i];
                line += L" ";
                line += Color::RESET;
            }
        }

        line += L"\033[10;15H";
        line += Color::BG_GRAY;
        line += Color::WHITE;
        line += L" Use Up/Down/PgUp/PgDn/Home/End to select, Enter/Space to toggle, ESC/F2 to exit. ";
        line += Color::RESET;

        m_renderer.Write(line);
    }

    void RenderSignalModal(SHORT width, SHORT height) {
        if (m_displayPids.empty()) return;

        DWORD targetPid = m_displayPids[m_selectedIndex];
        const auto& proc = m_telemetry.processes[targetPid];

        std::wstring line;
        line.reserve(512);
        line += L"\033[5;20H";
        line += Color::BG_RED;
        line += Color::WHITE;
        line += Color::BOLD;
        line += L" Send Signal to PID ";
        AppendFormat(line, L"%lu", proc.pid);
        line += L" (";
        line += proc.name;
        line += L") ";
        line += Color::RESET;

        const wchar_t* signals[] = {
            L" 15: SIGTERM  (Terminate Process)",
            L"  9: SIGKILL  (Force Kill Process)",
            L" 19: SIGSTOP  (Suspend Process)",
            L" 18: SIGCONT  (Resume Process)",
            L" Press 1-4 to select signal, ESC to cancel."
        };

        for (size_t i = 0; i < 5; ++i) {
            wchar_t posBuf[32];
            swprintf_s(posBuf, L"\033[%zu;20H", 6 + i);
            line += posBuf;
            line += Color::BG_GRAY;
            line += Color::WHITE;
            line += signals[i];
            line += Color::RESET;
        }

        m_renderer.Write(line);
    }

    // ========================================================================
    // INPUT HANDLING ENGINE
    // ========================================================================
    bool ProcessInput() {
        bool changed = false;
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        if (m_renderer.IsConsoleInput()) {
            DWORD numEvents = 0;
            while (GetNumberOfConsoleInputEvents(hIn, &numEvents) && numEvents > 0) {
                DWORD toRead = (std::min<DWORD>)(numEvents, 64);
                std::vector<INPUT_RECORD> records(toRead);
                DWORD read = 0;
                if (!ReadConsoleInputW(hIn, records.data(), toRead, &read)) break;

                for (DWORD i = 0; i < read; ++i) {
                    const auto& rec = records[i];
                    if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
                        HandleKeyEvent(rec.Event.KeyEvent);
                        changed = true;
                    } else if (rec.EventType == MOUSE_EVENT) {
                        HandleMouseEvent(rec.Event.MouseEvent);
                        changed = true;
                    }
                }
            }
            return changed;
        }

        // VT/PTY fallback path (VS Code terminal, Windows Terminal, etc.).
        while (_kbhit()) {
            wchar_t ch = static_cast<wchar_t>(_getwch());
            changed = true;

            if (ch == 0 || ch == 0xE0) {
                wchar_t ext = static_cast<wchar_t>(_getwch());
                KEY_EVENT_RECORD key = {};
                key.bKeyDown = TRUE;
                switch (ext) {
                    case 72: key.wVirtualKeyCode = VK_UP; break;
                    case 80: key.wVirtualKeyCode = VK_DOWN; break;
                    case 73: key.wVirtualKeyCode = VK_PRIOR; break;
                    case 81: key.wVirtualKeyCode = VK_NEXT; break;
                    case 71: key.wVirtualKeyCode = VK_HOME; break;
                    case 79: key.wVirtualKeyCode = VK_END; break;
                    case 59: key.wVirtualKeyCode = VK_F1; break;
                    case 60: key.wVirtualKeyCode = VK_F2; break;
                    case 61: key.wVirtualKeyCode = VK_F3; break;
                    case 62: key.wVirtualKeyCode = VK_F4; break;
                    case 63: key.wVirtualKeyCode = VK_F5; break;
                    case 64: key.wVirtualKeyCode = VK_F6; break;
                    case 65: key.wVirtualKeyCode = VK_F7; break;
                    case 66: key.wVirtualKeyCode = VK_F8; break;
                    case 67: key.wVirtualKeyCode = VK_F9; break;
                    case 68: key.wVirtualKeyCode = VK_F10; break;
                    default: key.wVirtualKeyCode = 0; break;
                }
                HandleKeyEvent(key);
                continue;
            }

            if (ch == 27) {
                std::wstring seq;
                while (_kbhit()) {
                    seq.push_back(static_cast<wchar_t>(_getwch()));
                }

                if (seq.empty()) {
                    KEY_EVENT_RECORD key = {};
                    key.bKeyDown = TRUE;
                    key.wVirtualKeyCode = VK_ESCAPE;
                    HandleKeyEvent(key);
                    continue;
                }

                if (seq.rfind(L"[<", 0) == 0) {
                    int btn = 0, x = 0, y = 0;
                    wchar_t action = 0;
                    if (swscanf_s(seq.c_str(), L"[<%d;%d;%d%c", &btn, &x, &y, &action, 1) == 4) {
                        MOUSE_EVENT_RECORD mouse = {};
                        mouse.dwMousePosition.X = static_cast<SHORT>((std::max)(0, x - 1));
                        mouse.dwMousePosition.Y = static_cast<SHORT>((std::max)(0, y - 1));

                        if (btn == 64 || btn == 65) {
                            mouse.dwEventFlags = MOUSE_WHEELED;
                            mouse.dwButtonState = (btn == 64) ? static_cast<DWORD>(120) : static_cast<DWORD>(-120);
                        } else if (action == L'M') {
                            mouse.dwEventFlags = 0;
                            mouse.dwButtonState = FROM_LEFT_1ST_BUTTON_PRESSED;
                        } else {
                            mouse.dwEventFlags = 0;
                            mouse.dwButtonState = 0;
                        }

                        HandleMouseEvent(mouse);
                    }
                    continue;
                }

                KEY_EVENT_RECORD key = {};
                key.bKeyDown = TRUE;
                if (seq == L"[A") key.wVirtualKeyCode = VK_UP;
                else if (seq == L"[B") key.wVirtualKeyCode = VK_DOWN;
                else if (seq == L"[5~") key.wVirtualKeyCode = VK_PRIOR;
                else if (seq == L"[6~") key.wVirtualKeyCode = VK_NEXT;
                else if (seq == L"[H" || seq == L"[1~") key.wVirtualKeyCode = VK_HOME;
                else if (seq == L"[F" || seq == L"[4~") key.wVirtualKeyCode = VK_END;
                else if (seq == L"OP" || seq == L"[11~") key.wVirtualKeyCode = VK_F1;
                else if (seq == L"OQ" || seq == L"[12~") key.wVirtualKeyCode = VK_F2;
                else if (seq == L"OR" || seq == L"[13~") key.wVirtualKeyCode = VK_F3;
                else if (seq == L"OS" || seq == L"[14~") key.wVirtualKeyCode = VK_F4;
                else if (seq == L"[15~") key.wVirtualKeyCode = VK_F5;
                else if (seq == L"[17~") key.wVirtualKeyCode = VK_F6;
                else if (seq == L"[18~") key.wVirtualKeyCode = VK_F7;
                else if (seq == L"[19~") key.wVirtualKeyCode = VK_F8;
                else if (seq == L"[20~") key.wVirtualKeyCode = VK_F9;
                else if (seq == L"[21~") key.wVirtualKeyCode = VK_F10;
                else key.wVirtualKeyCode = VK_ESCAPE;
                HandleKeyEvent(key);
                continue;
            }

            KEY_EVENT_RECORD key = {};
            key.bKeyDown = TRUE;
            key.uChar.UnicodeChar = ch;
            if (ch == L'\r') key.wVirtualKeyCode = VK_RETURN;
            else if (ch == L'\b') key.wVirtualKeyCode = VK_BACK;
            else key.wVirtualKeyCode = 0;
            HandleKeyEvent(key);
        }

        return changed;
    }

    void HandleKeyEvent(const KEY_EVENT_RECORD& key) {
        WORD vk = key.wVirtualKeyCode;
        WORD scan = key.wVirtualScanCode;
        wchar_t ch = key.uChar.UnicodeChar;
        bool shiftDown = (key.dwControlKeyState & SHIFT_PRESSED) != 0;

        // Some console hosts provide scan code with vk=0 for function/navigation keys.
        if (vk == 0) {
            switch (scan) {
                case 72: vk = VK_UP; break;
                case 80: vk = VK_DOWN; break;
                case 73: vk = VK_PRIOR; break;
                case 81: vk = VK_NEXT; break;
                case 71: vk = VK_HOME; break;
                case 79: vk = VK_END; break;
                case 59: vk = VK_F1; break;
                case 60: vk = VK_F2; break;
                case 61: vk = VK_F3; break;
                case 62: vk = VK_F4; break;
                case 63: vk = VK_F5; break;
                case 64: vk = VK_F6; break;
                case 65: vk = VK_F7; break;
                case 66: vk = VK_F8; break;
                case 67: vk = VK_F9; break;
                case 68: vk = VK_F10; break;
                default: break;
            }
        }

        // Modal Specific Input Processing
        if (m_currentMode == ViewMode::HelpModal) {
            if (vk == VK_ESCAPE || vk == VK_F1 || ch == L'h') m_currentMode = ViewMode::Normal;
            return;
        }

        if (m_currentMode == ViewMode::SetupModal) {
            switch (vk) {
                case VK_ESCAPE:
                case VK_F2:
                    m_currentMode = ViewMode::Normal;
                    break;
                case VK_UP:
                    if (m_setupSelectedOption > 0) m_setupSelectedOption--;
                    break;
                case VK_DOWN:
                    if (m_setupSelectedOption + 1 < kSetupOptionCount) m_setupSelectedOption++;
                    break;
                case VK_PRIOR:
                case VK_HOME:
                    m_setupSelectedOption = 0;
                    break;
                case VK_NEXT:
                case VK_END:
                    m_setupSelectedOption = kSetupOptionCount - 1;
                    break;
                case VK_RETURN:
                case VK_SPACE:
                    ToggleSetupOption(m_setupSelectedOption);
                    break;
                default:
                    if (ch == L'S') {
                        m_currentMode = ViewMode::Normal;
                    }
                    break;
            }
            return;
        }

        if (m_currentMode == ViewMode::SignalModal) {
            if (vk == VK_ESCAPE) {
                m_currentMode = ViewMode::Normal;
            } else if (ch == L'1') {
                ExecuteProcessSignal(15);
                m_currentMode = ViewMode::Normal;
            } else if (ch == L'2') {
                ExecuteProcessSignal(9);
                m_currentMode = ViewMode::Normal;
            } else if (ch == L'3') {
                ExecuteProcessSignal(19);
                m_currentMode = ViewMode::Normal;
            } else if (ch == L'4') {
                ExecuteProcessSignal(18);
                m_currentMode = ViewMode::Normal;
            }
            return;
        }

        if (m_currentMode == ViewMode::SearchInput) {
            if (vk == VK_ESCAPE) {
                m_currentMode = ViewMode::Normal;
            } else if (vk == VK_RETURN || vk == VK_F3) {
                if (!m_searchQuery.empty()) {
                    SelectSearchMatch(shiftDown ? false : true);
                }
            } else if (vk == VK_BACK && !m_searchQuery.empty()) {
                m_searchQuery.pop_back();
                ApplySearchSelection();
            } else if (ch >= 32) {
                m_searchQuery += ch;
                ApplySearchSelection();
            }
            return;
        }

        if (m_currentMode == ViewMode::FilterInput) {
            if (vk == VK_RETURN || vk == VK_ESCAPE) {
                m_currentMode = ViewMode::Normal;
            } else if (vk == VK_BACK && !m_filterQuery.empty()) {
                m_filterQuery.pop_back();
            } else if (ch >= 32) {
                m_filterQuery += ch;
            }
            return;
        }

        if (m_currentMode == ViewMode::UserFilterInput) {
            if (vk == VK_RETURN || vk == VK_ESCAPE) {
                m_currentMode = ViewMode::Normal;
            } else if (vk == VK_BACK && !m_userFilterQuery.empty()) {
                m_userFilterQuery.pop_back();
            } else if (ch >= 32) {
                m_userFilterQuery += ch;
            }
            return;
        }

        // Global Navigation & Commands
        switch (vk) {
            case VK_UP:
                if (m_selectedIndex > 0) m_selectedIndex--;
                break;
            case VK_DOWN:
                if (!m_displayPids.empty() && m_selectedIndex < m_displayPids.size() - 1) m_selectedIndex++;
                break;
            case VK_PRIOR: // Page Up
                if (m_selectedIndex >= 10) m_selectedIndex -= 10;
                else m_selectedIndex = 0;
                break;
            case VK_NEXT: // Page Down
                if (!m_displayPids.empty()) {
                    m_selectedIndex = (std::min)(m_displayPids.size() - 1, m_selectedIndex + 10);
                }
                break;
            case VK_HOME:
                m_selectedIndex = 0;
                break;
            case VK_END:
                if (!m_displayPids.empty()) m_selectedIndex = m_displayPids.size() - 1;
                break;

            case VK_SPACE:
                if (!m_displayPids.empty()) {
                    DWORD pid = m_displayPids[m_selectedIndex];
                    m_telemetry.processes[pid].isTagged = !m_telemetry.processes[pid].isTagged;
                }
                break;

            case VK_F1: m_currentMode = ViewMode::HelpModal; break;
            case VK_F2: m_currentMode = ViewMode::SetupModal; break;
            case VK_F3: m_currentMode = ViewMode::SearchInput; break;
            case VK_F4: m_currentMode = ViewMode::FilterInput; break;
            case VK_F5: m_treeView = !m_treeView; break;
            case VK_F6: CycleSortField(); break;
            case VK_F7: AdjustPriorityClass(false); break;
            case VK_F8: AdjustPriorityClass(true); break;
            case VK_F9: m_currentMode = ViewMode::SignalModal; break;
            case VK_F10: m_isRunning = false; break;

            default:
                if (ch == L'q') m_isRunning = false;
                if (ch == L'h') m_currentMode = ViewMode::HelpModal;
                if (ch == L'S') m_currentMode = ViewMode::SetupModal;
                if (ch == L't') m_treeView = !m_treeView;
                if (ch == L'k') m_currentMode = ViewMode::SignalModal;
                if (ch == L'u') m_currentMode = ViewMode::UserFilterInput;
                if (ch == L'U') ClearAllTags();
                if (ch == L'/') m_currentMode = ViewMode::SearchInput;
                if (ch == L'\\') m_currentMode = ViewMode::FilterInput;
                break;
        }
    }

    void HandleMouseEvent(const MOUSE_EVENT_RECORD& mouse) {
        if (mouse.dwEventFlags == MOUSE_WHEELED) {
            if (static_cast<int>(mouse.dwButtonState) > 0) {
                // Scroll Up
                if (m_selectedIndex >= 3) m_selectedIndex -= 3;
                else m_selectedIndex = 0;
            } else {
                // Scroll Down
                if (!m_displayPids.empty()) {
                    m_selectedIndex = (std::min)(m_displayPids.size() - 1, m_selectedIndex + 3);
                }
            }
        } else if (mouse.dwEventFlags == 0 && (mouse.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)) {
            // Click to select line
            SHORT clickY = mouse.dwMousePosition.Y;
            int headerLines = m_lastHeaderLines;
            if (clickY >= headerLines + 1) {
                size_t clickedIndex = m_scrollOffset + (clickY - headerLines - 1);
                if (clickedIndex < m_displayPids.size()) {
                    m_selectedIndex = clickedIndex;
                }
            }
        }
    }

    void CycleSortField() {
        int next = (static_cast<int>(m_sortField) + 1) % 11;
        m_sortField = static_cast<SortField>(next);
    }

    void ApplySearchSelection() {
        if (m_searchQuery.empty()) {
            m_searchHasMatch = true;
            m_searchMatchIndex = 0;
            m_searchMatchTotal = 0;
            return;
        }

        BuildProcessDisplayList();
        std::wstring needle = ToLowerCopy(m_searchQuery);

        size_t matchOrdinal = 0;

        for (size_t i = 0; i < m_displayPids.size(); ++i) {
            DWORD pid = m_displayPids[i];
            const auto& proc = m_telemetry.processes[pid];

            if (ProcessMatchesNeedle(proc, needle)) {
                matchOrdinal++;
                m_selectedIndex = i;
                m_searchHasMatch = true;
                RefreshSearchMatchStats(needle, matchOrdinal);
                return;
            }
        }

        m_searchHasMatch = false;
        m_searchMatchIndex = 0;
        m_searchMatchTotal = 0;
    }

    void SelectSearchMatch(bool forward) {
        if (m_searchQuery.empty()) {
            m_searchHasMatch = true;
            return;
        }

        BuildProcessDisplayList();
        if (m_displayPids.empty()) {
            m_searchHasMatch = false;
            return;
        }

        std::wstring needle = ToLowerCopy(m_searchQuery);
        size_t count = m_displayPids.size();
        size_t start = (m_selectedIndex < count) ? m_selectedIndex : 0;

        size_t totalMatches = 0;
        for (size_t i = 0; i < count; ++i) {
            DWORD pid = m_displayPids[i];
            const auto& proc = m_telemetry.processes[pid];
            if (ProcessMatchesNeedle(proc, needle)) {
                totalMatches++;
            }
        }

        for (size_t step = 1; step <= count; ++step) {
            size_t idx = forward
                ? (start + step) % count
                : (start + count - (step % count)) % count;

            DWORD pid = m_displayPids[idx];
            const auto& proc = m_telemetry.processes[pid];
            if (ProcessMatchesNeedle(proc, needle)) {
                m_selectedIndex = idx;
                m_searchHasMatch = true;

                if (totalMatches > 0) {
                    size_t ordinal = 0;
                    for (size_t i = 0; i < count; ++i) {
                        DWORD pid2 = m_displayPids[i];
                        const auto& proc2 = m_telemetry.processes[pid2];
                        if (!ProcessMatchesNeedle(proc2, needle)) continue;
                        ordinal++;
                        if (i == idx) break;
                    }
                    m_searchMatchIndex = ordinal;
                    m_searchMatchTotal = totalMatches;
                } else {
                    m_searchMatchIndex = 0;
                    m_searchMatchTotal = 0;
                }
                return;
            }
        }

        m_searchHasMatch = false;
        m_searchMatchIndex = 0;
        m_searchMatchTotal = 0;
    }

    void ToggleSetupOption(size_t optionIndex) {
        switch (optionIndex) {
            case 0:
                m_showThreads = !m_showThreads;
                break;
            case 1:
                m_useUnicodeTreeGraphics = !m_useUnicodeTreeGraphics;
                break;
            case 2:
                m_highlightProgramBasename = !m_highlightProgramBasename;
                break;
            case 3:
                m_autoRefreshTelemetry = !m_autoRefreshTelemetry;
                if (m_autoRefreshTelemetry) {
                    m_telemetry.UpdateSystemMetrics();
                }
                break;
            default:
                break;
        }
    }

    void ClearAllTags() {
        for (auto& [pid, proc] : m_telemetry.processes) {
            proc.isTagged = false;
        }
    }

    void AdjustPriorityClass(bool raise) {
        if (m_displayPids.empty()) return;
        DWORD targetPid = m_displayPids[m_selectedIndex];

        HANDLE hProcess = OpenProcess(PROCESS_SET_INFORMATION, FALSE, targetPid);
        if (!hProcess) return;

        DWORD currentClass = GetPriorityClass(hProcess);
        DWORD newClass = currentClass;

        if (raise) {
            if (currentClass == IDLE_PRIORITY_CLASS) newClass = BELOW_NORMAL_PRIORITY_CLASS;
            else if (currentClass == BELOW_NORMAL_PRIORITY_CLASS) newClass = NORMAL_PRIORITY_CLASS;
            else if (currentClass == NORMAL_PRIORITY_CLASS) newClass = ABOVE_NORMAL_PRIORITY_CLASS;
            else if (currentClass == ABOVE_NORMAL_PRIORITY_CLASS) newClass = HIGH_PRIORITY_CLASS;
        } else {
            if (currentClass == HIGH_PRIORITY_CLASS) newClass = ABOVE_NORMAL_PRIORITY_CLASS;
            else if (currentClass == ABOVE_NORMAL_PRIORITY_CLASS) newClass = NORMAL_PRIORITY_CLASS;
            else if (currentClass == NORMAL_PRIORITY_CLASS) newClass = BELOW_NORMAL_PRIORITY_CLASS;
            else if (currentClass == BELOW_NORMAL_PRIORITY_CLASS) newClass = IDLE_PRIORITY_CLASS;
        }

        if (newClass != currentClass) {
            SetPriorityClass(hProcess, newClass);
        }

        CloseHandle(hProcess);
    }

    void ExecuteProcessSignal(int sig) {
        if (m_displayPids.empty()) return;

        std::vector<DWORD> targetPids;
        // Check if any processes are tagged for batch execution
        for (const auto& [pid, proc] : m_telemetry.processes) {
            if (proc.isTagged) targetPids.push_back(pid);
        }

        if (targetPids.empty()) {
            targetPids.push_back(m_displayPids[m_selectedIndex]);
        }

        for (DWORD pid : targetPids) {
            if (sig == 15 || sig == 9) { // SIGTERM or SIGKILL
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                if (hProc) {
                    TerminateProcess(hProc, 1);
                    CloseHandle(hProc);
                }
            } else if (sig == 19) { // SIGSTOP (Suspend)
                HANDLE hProc = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
                if (hProc) {
                    NtSuspendProcess(hProc);
                    CloseHandle(hProc);
                }
            } else if (sig == 18) { // SIGCONT (Resume)
                HANDLE hProc = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
                if (hProc) {
                    NtResumeProcess(hProc);
                    CloseHandle(hProc);
                }
            }
        }
    }
};

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
int wmain() {
    int argc = __argc;
    wchar_t** argv = __wargv;

    if (argc > 1) {
        std::wstring arg1 = argv[1] ? argv[1] : L"";
        if (arg1 == L"--help" || arg1 == L"-h" || arg1 == L"/?") {
            std::wcout << LR"(htop(1)            CrossShell for UNIX Reference Manual                 htop(1)

    NAME
        htop - interactive process viewer and system resource monitor

    SYNOPSIS
        htop [OPTIONS]

    DESCRIPTION
        htop is an interactive real-time process monitoring and system management
        tool for Windows NT, displaying per-core CPU meters, memory gauges, process
        trees, and thread telemetry.

    OPTIONS
        -d, --delay DELAY
            Delay between updates in tenths of seconds.

        -u, --user USERNAME
            Show only processes owned by the specified user.

        -p, --pid PID...
            Show only processes with the specified process IDs.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    INTERACTIVE KEYS
        F1, h, ?
            Open interactive help screen.

        F2, S
            Open setup and meter configuration.

        F3, /
            Incremental process search.

        F4, \
            Incremental process filter.

        F5, t
            Toggle tree view hierarchy.

        F6, <, >
            Sort by selected column.

        F7, ]
            Increase process priority.

        F8, [
            Decrease process priority.

        F9, k
            Send signal or terminate selected process.

        F10, q
            Quit the application.

    EXAMPLES
        htop
            Launch interactive process monitor.

        htop -d 5
            Launch with a 0.5-second update interval.

        htop -u SYSTEM
            Show processes running under the SYSTEM account.

    CrossShell for UNIX                                                    htop(1)
)";
            return 0;
        }
        if (arg1 == L"--version" || arg1 == L"-V") {
            std::wcout << AppInfo::NAME << L" " << AppInfo::VERSION << L"\n";
            return 0;
        }
    }

    if (!InitializeNtApis()) {
        std::wcerr << L"Error: Failed to bind Native NTDLL APIs.\n";
        return 1;
    }

    HtopApp app;
    app.Run();

    return 0;
}