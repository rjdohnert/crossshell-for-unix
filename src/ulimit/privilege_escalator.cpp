#include "privilege_escalator.hpp"
#include "scoped_token_handle.hpp"

bool PrivilegeEscalator::EnableDebugPrivilege() {
        ScopedTokenHandle token;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, token.Receive())) return false;
        LUID luid;
        if (!LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid)) return false;
        TOKEN_PRIVILEGES privileges = {};
        privileges.PrivilegeCount = 1;
        privileges.Privileges[0].Luid = luid;
        privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if (!AdjustTokenPrivileges(token.Get(), FALSE, &privileges, sizeof(privileges), nullptr, nullptr)) return false;
        return GetLastError() != ERROR_NOT_ALL_ASSIGNED;
    }
