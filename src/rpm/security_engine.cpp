#include "security_engine.hpp"

bool SecurityEngine::isAdministrator() {
        BOOL isAdmin = FALSE;
        PSID adminGroup = NULL;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                      DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
            CheckTokenMembership(NULL, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }
        return isAdmin == TRUE;
    }

bool SecurityEngine::applyPosixPermissions(const fs::path& filePath, uint32_t posixMode) {
        DWORD permissions = GENERIC_READ;
        if (posixMode & 0002) permissions |= GENERIC_WRITE;
        if ((posixMode & 0001) || (posixMode & 0100)) permissions |= GENERIC_EXECUTE;

        PSID pAdminSid = NULL, pEveryoneSid = NULL;
        SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
        SID_IDENTIFIER_AUTHORITY worldAuth = SECURITY_WORLD_SID_AUTHORITY;

        if (!AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdminSid) ||
            !AllocateAndInitializeSid(&worldAuth, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &pEveryoneSid)) {
            if (pAdminSid) FreeSid(pAdminSid);
            return false;
        }

        EXPLICIT_ACCESS_W ea[2] = {0};
        ea[0].grfAccessPermissions = GENERIC_ALL;
        ea[0].grfAccessMode = SET_ACCESS;
        ea[0].grfInheritance = NO_INHERITANCE;
        ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[0].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
        ea[0].Trustee.ptstrName = (LPWSTR)pAdminSid;

        ea[1].grfAccessPermissions = permissions;
        ea[1].grfAccessMode = SET_ACCESS;
        ea[1].grfInheritance = NO_INHERITANCE;
        ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[1].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea[1].Trustee.ptstrName = (LPWSTR)pEveryoneSid;

        PACL pNewDacl = NULL;
        DWORD dwRes = SetEntriesInAclW(2, ea, NULL, &pNewDacl);
        if (dwRes == ERROR_SUCCESS) {
            std::wstring wpath = filePath.wstring();
            dwRes = SetNamedSecurityInfoW(wpath.data(), SE_FILE_OBJECT,
                                          DACL_SECURITY_INFORMATION, NULL, NULL, pNewDacl, NULL);
            LocalFree(pNewDacl);
        }

        FreeSid(pAdminSid);
        FreeSid(pEveryoneSid);
        return dwRes == ERROR_SUCCESS;
    }
