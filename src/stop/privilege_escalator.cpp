#include "privilege_escalator.hpp"
#include "scoped_token_handle.hpp"

bool PrivilegeEscalator::EnableDebugPrivilege() {
        ScopedTokenHandle hToken;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, hToken.Receive()))
            return false;

        LUID luid = {};
        if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
            return false;
        }

        TOKEN_PRIVILEGES tp = {};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        BOOL res = AdjustTokenPrivileges(hToken.Get(), FALSE, &tp, sizeof(tp), NULL, NULL);
        return res && (GetLastError() != ERROR_NOT_ALL_ASSIGNED);
    }
