#include "security_utils.hpp"
#include "string_utils.hpp"

std::optional<std::wstring> SecurityUtils::GetUserSidString(const std::string& username) {
        std::wstring wUsername = StringUtils::ToWide(username);
        DWORD sidSize = 0;
        DWORD domainSize = 0;
        SID_NAME_USE sidType;

        LookupAccountNameW(NULL, wUsername.c_str(), NULL, &sidSize, NULL, &domainSize, &sidType);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return std::nullopt;
        }

        std::vector<BYTE> sidBuffer(sidSize);
        std::vector<WCHAR> domainBuffer(domainSize);
        PSID pSid = reinterpret_cast<PSID>(sidBuffer.data());

        if (!LookupAccountNameW(NULL, wUsername.c_str(), pSid, &sidSize, domainBuffer.data(), &domainSize, &sidType)) {
            return std::nullopt;
        }

        LPWSTR stringSid = NULL;
        if (ConvertSidToStringSidW(pSid, &stringSid)) {
            std::wstring result(stringSid);
            LocalFree(stringSid);
            return result;
        }

        return std::nullopt;
    }
