#include "network_utils.hpp"

bool NetworkUtils::IsUserAdmin() {
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

WORD NetworkUtils::CalculateChecksum(WORD* buffer, int size) {
        DWORD cksum = 0;
        while (size > 1) {
            cksum += *buffer++;
            size -= sizeof(WORD);
        }
        if (size) {
            cksum += *(BYTE*)buffer;
        }
        cksum = (cksum >> 16) + (cksum & 0xffff);
        cksum += (cksum >> 16);
        return (WORD)(~cksum);
    }

std::string NetworkUtils::ResolveHostname(IN_ADDR addr, bool numericMode) {
        char ipStr[INET_ADDRSTRLEN] = { 0 };
        inet_ntop(AF_INET, &addr, ipStr, INET_ADDRSTRLEN);

        if (numericMode) return std::string(ipStr);

        sockaddr_in sa = {};
        sa.sin_family = AF_INET;
        sa.sin_addr = addr;

        char hostBuf[NI_MAXHOST] = { 0 };
        if (getnameinfo((sockaddr*)&sa, sizeof(sa), hostBuf, sizeof(hostBuf), NULL, 0, NI_NAMEREQD) == 0) {
            return std::string(hostBuf) + " (" + std::string(ipStr) + ")";
        }
        return std::string(ipStr);
    }
