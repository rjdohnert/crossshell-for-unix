#include "environment_store.hpp"

std::map<std::wstring, std::wstring> EnvironmentStore::GetProcessEnvironmentMap() {
        std::map<std::wstring, std::wstring> envMap;
        LPWCH envBlock = GetEnvironmentStringsW();
        if (!envBlock) return envMap;

        LPCWSTR p = envBlock;
        while (*p) {
            std::wstring entry(p);
            if (!entry.empty() && entry[0] != L'=') {
                size_t eqPos = entry.find(L'=');
                if (eqPos != std::wstring::npos) {
                    envMap[entry.substr(0, eqPos)] = entry.substr(eqPos + 1);
                }
            }
            p += wcslen(p) + 1;
        }
        FreeEnvironmentStringsW(envBlock);
        return envMap;
    }

bool EnvironmentStore::SetProcessEnvironmentVariable(const std::wstring& var, const std::wstring& val) {
        return SetEnvironmentVariableW(var.c_str(), val.empty() ? nullptr : val.c_str()) != 0;
    }
