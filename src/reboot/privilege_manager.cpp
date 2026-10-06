#include "privilege_manager.hpp"
#include "scoped_sid_handle.hpp"
#include "scoped_token_handle.hpp"

bool PrivilegeManager::EnableShutdownPrivilege() {
        ScopedTokenHandle hToken;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, hToken.Receive())) {
            return false;
        }

        TOKEN_PRIVILEGES tkp = {};
        if (!LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid)) {
            return false;
        }

        tkp.PrivilegeCount = 1;
        tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        if (!AdjustTokenPrivileges(hToken.Get(), FALSE, &tkp, 0, nullptr, nullptr)) {
            return false;
        }

        return (GetLastError() == ERROR_SUCCESS);
    }

bool PrivilegeManager::IsRunningAsAdmin() {
        BOOL isAdmin = FALSE;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        ScopedSidHandle adminGroup;

        if (AllocateAndInitializeSid(
                &ntAuthority,
                2,
                SECURITY_BUILTIN_DOMAIN_RID,
                DOMAIN_ALIAS_RID_ADMINS,
                0,
                0,
                0,
                0,
                0,
                0,
                adminGroup.Receive())) {
            CheckTokenMembership(NULL, adminGroup.Get(), &isAdmin);
        }

        return isAdmin == TRUE;
    }
