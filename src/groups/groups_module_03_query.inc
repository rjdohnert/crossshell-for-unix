    class WindowsGroupQuery {
    public:
        struct QueryResult {
            bool success;
            unsigned long errorCode;
            std::string message;
            std::vector<GroupInfo> groups;
        };

        static std::string GetCurrentUsername() {
            WCHAR buffer[UNLEN + 1];
            DWORD size = UNLEN + 1;
            if (GetUserNameW(buffer, &size)) {
                return StringConverter::ToNarrow(buffer);
            }
            return "UNKNOWN_USER";
        }

        static QueryResult QueryUserGroups(const std::string& username, bool fetchSids) {
            if (username.empty()) {
                return { false, ERROR_INVALID_PARAMETER, "Username cannot be empty.", {} };
            }

            std::wstring wUsername = StringConverter::ToWide(username);
            std::set<GroupInfo> collectedGroups;

            // 1. Query Local Groups (including indirect memberships via global groups)
            LPLOCALGROUP_USERS_INFO_0 pLocalBuf = nullptr;
            DWORD localEntriesRead = 0;
            DWORD localTotalEntries = 0;

            NET_API_STATUS status = NetUserGetLocalGroups(
                nullptr,
                wUsername.c_str(),
                0,
                LG_INCLUDE_INDIRECT,
                reinterpret_cast<LPBYTE*>(&pLocalBuf),
                MAX_PREFERRED_LENGTH,
                &localEntriesRead,
                &localTotalEntries
            );

            if (status == NERR_UserNotFound) {
                return { false, status, "no such user", {} };
            }

            if (status != NERR_Success && status != ERROR_MORE_DATA) {
                return { false, status, ResolveErrorMessage(status), {} };
            }

            if (pLocalBuf != nullptr) {
                for (DWORD i = 0; i < localEntriesRead; ++i) {
                    std::string gName = StringConverter::ToNarrow(pLocalBuf[i].lgrui0_name);
                    std::string sidStr = fetchSids ? LookupGroupSid(pLocalBuf[i].lgrui0_name) : "";
                    collectedGroups.insert(GroupInfo{ gName, sidStr, true });
                }
                NetApiBufferFree(pLocalBuf);
            }

            // 2. Query Global Groups
            LPGROUP_USERS_INFO_0 pGlobalBuf = nullptr;
            DWORD globalEntriesRead = 0;
            DWORD globalTotalEntries = 0;

            status = NetUserGetGroups(
                nullptr,
                wUsername.c_str(),
                0,
                reinterpret_cast<LPBYTE*>(&pGlobalBuf),
                MAX_PREFERRED_LENGTH,
                &globalEntriesRead,
                &globalTotalEntries
            );

            if (status == NERR_Success && pGlobalBuf != nullptr) {
                for (DWORD i = 0; i < globalEntriesRead; ++i) {
                    std::string gName = StringConverter::ToNarrow(pGlobalBuf[i].grui0_name);
                    std::string sidStr = fetchSids ? LookupGroupSid(pGlobalBuf[i].grui0_name) : "";
                    collectedGroups.insert(GroupInfo{ gName, sidStr, false });
                }
                NetApiBufferFree(pGlobalBuf);
            }

            std::vector<GroupInfo> result(collectedGroups.begin(), collectedGroups.end());
            return { true, 0, "Success", std::move(result) };
        }

    private:
        static std::string LookupGroupSid(LPCWSTR groupName) {
            DWORD cbSid = 0;
            DWORD cchDomain = 0;
            SID_NAME_USE peUse;

            LookupAccountNameW(nullptr, groupName, nullptr, &cbSid, nullptr, &cchDomain, &peUse);
            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                return "";
            }

            std::vector<BYTE> sidBuffer(cbSid);
            std::vector<WCHAR> domainBuffer(cchDomain);
            PSID pSid = reinterpret_cast<PSID>(sidBuffer.data());

            if (LookupAccountNameW(nullptr, groupName, pSid, &cbSid, domainBuffer.data(), &cchDomain, &peUse)) {
                LPWSTR stringSid = nullptr;
                if (ConvertSidToStringSidW(pSid, &stringSid)) {
                    std::string result = StringConverter::ToNarrow(stringSid);
                    LocalFree(stringSid);
                    return result;
                }
            }
            return "";
        }

        static std::string ResolveErrorMessage(DWORD errorCode) {
            switch (errorCode) {
            case ERROR_ACCESS_DENIED:
                return "Access denied. Insufficient permissions.";
            case NERR_InvalidComputer:
                return "The computer name is invalid.";
            case NERR_UserNotFound:
                return "no such user";
            default: {
                LPWSTR buffer = nullptr;
                size_t size = FormatMessageW(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    reinterpret_cast<LPWSTR>(&buffer), 0, nullptr
                );
                std::string msg = size ? StringConverter::ToNarrow(buffer) : "Unknown Windows API error.";
                if (buffer) LocalFree(buffer);
                return StringConverter::Trim(msg);
            }
            }
        }
    };

    // Command-line argument parser
