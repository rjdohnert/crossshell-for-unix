#pragma once

#include "strace.hpp"
#include "options.hpp"
#include "reporter.hpp"

class SymbolResolver {
public:
    static void Initialize(HANDLE hProcess);
    static void Cleanup(HANDLE hProcess);
    static std::string Resolve(HANDLE hProcess, DWORD64 address);
};

class ProcessTracker {
public:
    std::unordered_map<DWORD, HANDLE> processHandles;
    std::unordered_map<DWORD, HANDLE> threadHandles;
    size_t activeProcessCount = 0;

    ~ProcessTracker();
    void CleanupAll();
};

class TraceEngine {
public:
    static int Execute(TraceOptions& options);
};
