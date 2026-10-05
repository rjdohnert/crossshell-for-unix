// ============================================================================
// 2. SID RESOLVER & IDENTITY RETRIEVER
// ============================================================================

class SidResolver {
public:
    static AccountInfo Resolve(PSID pSid) {
        AccountInfo info;
        if (!pSid || !IsValidSid(pSid)) return info;

        LPWSTR strSid = nullptr;
        if (ConvertSidToStringSidW(pSid, &strSid)) {
            info.sidString = strSid;
            LocalFree(strSid);
        }

        wchar_t name[256] = { 0 };
        wchar_t domain[256] = { 0 };
        DWORD nameSize = 256;
        DWORD domainSize = 256;
        SID_NAME_USE sidUse;

        if (LookupAccountSidW(nullptr, pSid, name, &nameSize, domain, &domainSize, &sidUse)) {
            if (domainSize > 0 && wcslen(domain) > 0) {
                info.accountName = std::wstring(domain) + L"\\" + std::wstring(name);
            } else {
                info.accountName = name;
            }
        } else {
            info.accountName = info.sidString;
        }

        return info;
    }
};

class IdentityEngine {
public:
    static UserTokenData GetCurrentProcessData() {
        UserTokenData data;
        ScopedTokenHandle hToken;

        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, hToken.ReceiveHandle())) {
            return data;
        }

        DWORD len = 0;
        GetTokenInformation(hToken.Get(), TokenUser, nullptr, 0, &len);
        if (len > 0) {
            std::vector<BYTE> buffer(len);
            if (GetTokenInformation(hToken.Get(), TokenUser, buffer.data(), len, &len)) {
                PTOKEN_USER pUser = reinterpret_cast<PTOKEN_USER>(buffer.data());
                data.user = SidResolver::Resolve(pUser->User.Sid);
            }
        }

        len = 0;
        GetTokenInformation(hToken.Get(), TokenPrimaryGroup, nullptr, 0, &len);
        if (len > 0) {
            std::vector<BYTE> buffer(len);
            if (GetTokenInformation(hToken.Get(), TokenPrimaryGroup, buffer.data(), len, &len)) {
                PTOKEN_PRIMARY_GROUP pGroup = reinterpret_cast<PTOKEN_PRIMARY_GROUP>(buffer.data());
                data.primaryGroup = SidResolver::Resolve(pGroup->PrimaryGroup);
            }
        }

        len = 0;
        GetTokenInformation(hToken.Get(), TokenGroups, nullptr, 0, &len);
        if (len > 0) {
            std::vector<BYTE> buffer(len);
            if (GetTokenInformation(hToken.Get(), TokenGroups, buffer.data(), len, &len)) {
                PTOKEN_GROUPS pGroups = reinterpret_cast<PTOKEN_GROUPS>(buffer.data());
                for (DWORD i = 0; i < pGroups->GroupCount; ++i) {
                    data.groups.push_back(SidResolver::Resolve(pGroups->Groups[i].Sid));
                }
            }
        }

        return data;
    }

    static UserTokenData GetUserDataByName(const std::wstring& username) {
        UserTokenData data;
        DWORD sidSize = 0, domainSize = 0;
        SID_NAME_USE use;

        LookupAccountNameW(nullptr, username.c_str(), nullptr, &sidSize, nullptr, &domainSize, &use);
        if (sidSize == 0) return data;

        std::vector<BYTE> sidBuf(sidSize);
        std::vector<wchar_t> domainBuf(domainSize);

        if (!LookupAccountNameW(nullptr, username.c_str(), sidBuf.data(), &sidSize, domainBuf.data(), &domainSize, &use)) {
            return data;
        }

        data.user = SidResolver::Resolve(sidBuf.data());

        LPLOCALGROUP_USERS_INFO_0 pGroupInfo = nullptr;
        DWORD entriesRead = 0, totalEntries = 0;
        NET_API_STATUS status = NetUserGetLocalGroups(
            nullptr, username.c_str(), 0, LG_INCLUDE_INDIRECT,
            reinterpret_cast<LPBYTE*>(&pGroupInfo), MAX_PREFERRED_LENGTH,
            &entriesRead, &totalEntries
        );

        if (status == NERR_Success && pGroupInfo) {
            for (DWORD i = 0; i < entriesRead; ++i) {
                DWORD gSidSize = 0, gDomSize = 0;
                SID_NAME_USE gUse;
                LookupAccountNameW(nullptr, pGroupInfo[i].lgrui0_name, nullptr, &gSidSize, nullptr, &gDomSize, &gUse);
                if (gSidSize > 0) {
                    std::vector<BYTE> gSidBuf(gSidSize);
                    std::vector<wchar_t> gDomBuf(gDomSize);
                    if (LookupAccountNameW(nullptr, pGroupInfo[i].lgrui0_name, gSidBuf.data(), &gSidSize, gDomBuf.data(), &gDomSize, &gUse)) {
                        data.groups.push_back(SidResolver::Resolve(gSidBuf.data()));
                    }
                }
            }
            NetApiBufferFree(pGroupInfo);
        }

        if (!data.groups.empty()) {
            data.primaryGroup = data.groups[0];
        } else {
            data.primaryGroup = data.user;
        }

        return data;
    }
};

