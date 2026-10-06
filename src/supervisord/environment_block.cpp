#include "environment_block.hpp"

std::vector<wchar_t> CreateEnvironmentBlock(const std::map<std::wstring, std::wstring>& customEnv) {
    LPWCH sysEnv = GetEnvironmentStringsW();
    std::map<std::wstring, std::wstring> envMap;

    LPWCH var = sysEnv;
    while (*var) {
        std::wstring s(var);
        size_t eq = s.find(L'=');
        if (eq != std::wstring::npos && eq > 0) {
            envMap[s.substr(0, eq)] = s.substr(eq + 1);
        }
        var += s.length() + 1;
    }
    FreeEnvironmentStringsW(sysEnv);

    for (const auto& [k, v] : customEnv) {
        envMap[k] = v;
    }

    std::vector<wchar_t> block;
    for (const auto& [k, v] : envMap) {
        std::wstring entry = k + L"=" + v;
        block.insert(block.end(), entry.begin(), entry.end());
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}
