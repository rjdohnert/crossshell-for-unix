#include "security_inspector.hpp"

std::wstring SecurityInspector::getFileOwner(const std::wstring& path) {
        DWORD lengthNeeded = 0;
        // Query required buffer size for the security descriptor
        GetFileSecurityW(path.c_str(), OWNER_SECURITY_INFORMATION, nullptr, 0, &lengthNeeded);
        if (lengthNeeded == 0) {
            return L"UNKNOWN";
        }

        std::vector<BYTE> sdBuffer(lengthNeeded);
        auto pSD = reinterpret_cast<PSECURITY_DESCRIPTOR>(sdBuffer.data());

        if (!GetFileSecurityW(path.c_str(), OWNER_SECURITY_INFORMATION, pSD, lengthNeeded, &lengthNeeded)) {
            return L"UNKNOWN";
        }

        PSID pOwnerSid = nullptr;
        BOOL bOwnerDefaulted = FALSE;
        if (!GetSecurityDescriptorOwner(pSD, &pOwnerSid, &bOwnerDefaulted) || !pOwnerSid) {
            return L"UNKNOWN";
        }

        WCHAR nameBuffer[256] = { 0 };
        DWORD nameLen = 256;
        WCHAR domainBuffer[256] = { 0 };
        DWORD domainLen = 256;
        SID_NAME_USE sidType;

        if (LookupAccountSidW(nullptr, pOwnerSid, nameBuffer, &nameLen, domainBuffer, &domainLen, &sidType)) {
            if (domainLen > 0) {
                return std::wstring(domainBuffer) + L"\\" + nameBuffer;
            }
            return std::wstring(nameBuffer);
        }

        return L"UNKNOWN";
    }
