#include "grep_backend_locator.hpp"

std::wstring GrepBackendLocator::GetModuleDirectory() {
        wchar_t path[MAX_PATH] = {};
        DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) return L"";

        std::wstring full(path, len);
        size_t slash = full.find_last_of(L"\\/");
        if (slash == std::wstring::npos) return L"";
        return full.substr(0, slash);
    }

std::wstring GrepBackendLocator::Resolve() const {
        std::wstring local = GetModuleDirectory();
        if (!local.empty()) {
            local += L"\\grep.exe";
            DWORD attrs = GetFileAttributesW(local.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                return local;
            }
        }
        return L"grep.exe";
    }
