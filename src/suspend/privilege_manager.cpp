#include "privilege_manager.hpp"

bool PrivilegeManager::EnableDebugPrivilege() {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
            return false;
        }

        LUID luid;
        if (!LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid)) {
            CloseHandle(token);
            return false;
        }

        TOKEN_PRIVILEGES privileges = {};
        privileges.PrivilegeCount = 1;
        privileges.Privileges[0].Luid = luid;
        privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        if (!AdjustTokenPrivileges(token, FALSE, &privileges, sizeof(privileges), nullptr, nullptr)) {
            CloseHandle(token);
            return false;
        }

        const bool ok = GetLastError() != ERROR_NOT_ALL_ASSIGNED;
        CloseHandle(token);
        return ok;
    }
