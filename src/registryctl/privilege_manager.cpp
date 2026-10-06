#include "privilege_manager.hpp"

bool Security::EnableRequiredPrivileges() {
        HANDLE hToken = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            return false;
        }
        
        auto SetPrivilege = [&](LPCWSTR privName) {
            TOKEN_PRIVILEGES tp;
            LUID luid;
            if (LookupPrivilegeValueW(nullptr, privName, &luid)) {
                tp.PrivilegeCount = 1;
                tp.Privileges[0].Luid = luid;
                tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr);
            }
        };

        SetPrivilege(SE_BACKUP_NAME);
        SetPrivilege(SE_RESTORE_NAME);
        CloseHandle(hToken);
        return true;
    }
