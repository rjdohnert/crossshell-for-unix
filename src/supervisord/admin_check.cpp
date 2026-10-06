#include "admin_check.hpp"

bool IsTokenElevatedOrAdmin(HANDLE token) {
    TOKEN_ELEVATION elevation = {0};
    DWORD bytes = 0;
    if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &bytes) && elevation.TokenIsElevated) {
        return true;
    }

    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID adminGroup = NULL;
    BOOL isAdmin = FALSE;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(token, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

bool IsCurrentProcessElevatedOrAdmin() {
    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    bool ok = IsTokenElevatedOrAdmin(token);
    CloseHandle(token);
    return ok;
}
