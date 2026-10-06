#include "scoped_process_token.hpp"
#include "scoped_sid.hpp"
#include "vhd_storage_inspector.hpp"

std::wstring VhdStorageInspector::CsvQuote(const std::wstring& value) {
        std::wstring result = L"\"";
        for (wchar_t ch : value) { if (ch == L'"') result += L"\"\""; else result += ch; }
        return result + L"\"";
    }

std::wstring VhdStorageInspector::GetErrorMessage(DWORD errorCode) {
        LPWSTR buffer = nullptr;
        DWORD size = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&buffer, 0, NULL);
        
        if (size == 0 || !buffer) {
            return L"Unknown error code " + std::to_wstring(errorCode);
        }

        std::wstring message(buffer, size);
        LocalFree(buffer);
        
        while (!message.empty() && (message.back() == L'\n' || message.back() == L'\r' || message.back() == L' ')) {
            message.pop_back();
        }
        return message;
    }

bool VhdStorageInspector::IsAdmin() {
        HANDLE token = NULL;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            ScopedProcessToken scopedToken(token);
            TOKEN_ELEVATION elevation = {};
            DWORD size = 0;
            if (GetTokenInformation(scopedToken.Get(), TokenElevation, &elevation, sizeof(elevation), &size)) {
                return elevation.TokenIsElevated != 0;
            }
        }

        BOOL isAdmin = FALSE;
        PSID adminGroup = NULL;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                     DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
            ScopedSid scopedSid(adminGroup);
            CheckTokenMembership(NULL, scopedSid.Get(), &isAdmin);
        }
        return isAdmin == TRUE;
    }

ULONG VhdStorageInspector::GetDeviceType(const std::wstring& path) {
        if (path.empty()) return VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN;
        const size_t dot = path.find_last_of(L'.');
        if (dot == std::wstring::npos || dot + 1 >= path.size()) return VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN;
        std::wstring ext = path.substr(dot);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        if (ext == L".vhdx") return VIRTUAL_STORAGE_TYPE_DEVICE_VHDX;
        if (ext == L".vhd") return VIRTUAL_STORAGE_TYPE_DEVICE_VHD;
        return VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN;
    }

ULONGLONG VhdStorageInspector::ParseSize(const std::wstring& str) {
        if (str.empty()) return 0;

        std::wstring trimmed = str;
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](wchar_t ch) {
            return !std::iswspace(ch);
        }));
        trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](wchar_t ch) {
            return !std::iswspace(ch);
        }).base(), trimmed.end());

        if (trimmed.empty()) return 0;

        wchar_t unit = std::towupper(trimmed.back());
        ULONGLONG multiplier = 1;
        std::wstring numStr = trimmed;

        if (unit == L'K') { multiplier = 1024ULL; numStr.pop_back(); }
        else if (unit == L'M') { multiplier = 1024ULL * 1024ULL; numStr.pop_back(); }
        else if (unit == L'G') { multiplier = 1024ULL * 1024ULL * 1024ULL; numStr.pop_back(); }
        else if (unit == L'T') { multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL; numStr.pop_back(); }

        if (numStr.empty()) return 0;

        try {
            unsigned long long value = std::stoull(numStr);
            const unsigned long long maxValue = ~0ULL;
            if (value > (maxValue / multiplier)) return 0;
            return value * multiplier;
        } catch (...) {
            return 0;
        }
    }
