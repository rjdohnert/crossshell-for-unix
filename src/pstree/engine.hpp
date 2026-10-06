#pragma once

#include "pstree.hpp"

class ProcessTreeEngine {
public:
    static bool EnableVirtualTerminalAndUnicode() noexcept;
    static std::wstring QueryProcessImagePath(DWORD pid) noexcept;
    static bool SnapshotProcesses(ProcessMap& outMap) noexcept;
};
