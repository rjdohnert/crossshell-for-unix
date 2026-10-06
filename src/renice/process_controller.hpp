#pragma once

#include "process_info.hpp"
#include "renice.hpp"

class ProcessController {
public:
    [[nodiscard]] static std::wstring getProcessOwner(HANDLE hProcess);

    [[nodiscard]] static std::vector<ProcessInfo> snapshotProcesses();

    static bool applyPriority(DWORD pid, DWORD targetPriority, std::string& errorMsg);
};
