#pragma once

#include "pskill.hpp"
#include "options.hpp"

class ProcessKillerEngine {
public:
    static std::wstring GetProcessOwner(DWORD pid);
    static std::vector<ProcessEntry> GetProcessList();
    static bool SendWMClose(DWORD pid);
    static void CollectTreePostOrder(DWORD pid, const std::vector<ProcessEntry>& allProcs, std::vector<DWORD>& orderedPids, std::set<DWORD>& visited);
    static bool TerminateSingleProcess(const ProcessEntry& proc, const PskillOptions& config);
    static int Execute(const PskillOptions& config);

private:
    struct EnumData { DWORD pid; bool sent; };
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
};
