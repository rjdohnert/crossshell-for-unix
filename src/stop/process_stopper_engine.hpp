#pragma once

#include "stop_options.hpp"
#include "stop.hpp"

class ProcessStopperEngine {
public:
    struct EnumData {
        DWORD pid;
        bool windowFound;
    };

    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);

    static bool ForceStopProcess(DWORD pid);

    static bool GracefulStopProcess(DWORD pid);

    static bool SendConsoleEvent(DWORD pid, DWORD ctrlEvent);

    static int Execute(const StopOptions& options);
};
