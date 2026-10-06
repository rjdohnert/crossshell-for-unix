#pragma once

#include "suspend.hpp"

class ProcessSuspendEngine {
private:
    static bool OperateWithNtdll(HANDLE process_handle, bool resume);

    static bool OperateWithThreads(DWORD pid, bool resume);

public:
    static bool OperateOnPid(DWORD pid, bool resume, bool use_ntdll);
};
