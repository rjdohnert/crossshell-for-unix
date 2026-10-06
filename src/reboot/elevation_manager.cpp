#include "elevation_manager.hpp"

bool ElevationManager::RelaunchElevated() {
        char exePath[MAX_PATH] = {0};
        if (GetModuleFileNameA(NULL, exePath, MAX_PATH) == 0) {
            return false;
        }

        SHELLEXECUTEINFOA sei = {0};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.hwnd = NULL;
        sei.lpVerb = "runas";
        sei.lpFile = exePath;
        sei.lpParameters = "--elevated";
        sei.nShow = SW_SHOWNORMAL;

        return ShellExecuteExA(&sei) != FALSE;
    }
