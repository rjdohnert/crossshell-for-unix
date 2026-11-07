#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#pragma once

#include <windows.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
    SignalConfirmModal,
    UserFilterInput
};

enum class ProcessAction {
    Terminate,
    Suspend,
    Resume
};

struct ProcessIdentity {
    DWORD pid = 0;
    ULONGLONG creationTime = 0;
};

struct HtopOptions {
    int delayTenths = 15;
    std::wstring userFilter;
    std::unordered_set<DWORD> processIds;
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
    std::wstring user;
    LONG priority = 0;
    SIZE_T virtMem = 0;
    SIZE_T resMem = 0;
    SIZE_T sharedMem = 0;
    ULONGLONG creationTime = 0;
    wchar_t state = L'R';
    double cpuPercent = 0.0;
    double memPercent = 0.0;
    ULONGLONG totalCpuTime = 0;
    DWORD threadCount = 0;
    bool isTagged = false;
    std::wstring treePrefix;
    uint64_t seenGeneration = 0;
};
