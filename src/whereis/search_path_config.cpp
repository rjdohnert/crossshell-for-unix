#include "search_path_config.hpp"

std::wstring SearchPathConfig::getEnvVar(const wchar_t* varName) {
        DWORD size = GetEnvironmentVariableW(varName, NULL, 0);
        if (size == 0) return L"";
        std::vector<wchar_t> buffer(size);
        GetEnvironmentVariableW(varName, buffer.data(), size);
        return std::wstring(buffer.data());
    }

std::vector<std::wstring> SearchPathConfig::split(const std::wstring& str, wchar_t delim) {
        std::vector<std::wstring> tokens;
        std::wstringstream ss(str);
        std::wstring item;
        while (std::getline(ss, item, delim)) {
            if (!item.empty()) tokens.push_back(item);
        }
        return tokens;
    }

void SearchPathConfig::initializeDefaults() {
        binDirs.push_back(L".");
        std::wstring pathEnv = getEnvVar(L"PATH");
        if (!pathEnv.empty()) {
            std::vector<std::wstring> paths = split(pathEnv, L';');
            binDirs.insert(binDirs.end(), paths.begin(), paths.end());
        }

        binExts.push_back(L"");
        std::wstring pathextEnv = getEnvVar(L"PATHEXT");
        if (!pathextEnv.empty()) {
            std::vector<std::wstring> exts = split(pathextEnv, L';');
            for (auto& ext : exts) {
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                binExts.push_back(ext);
            }
        } else {
            binExts.insert(binExts.end(), { L".exe", L".cmd", L".bat", L".com", L".ps1" });
        }

        manDirs = { L".", L"doc", L"docs", L"man", L"help" };
        srcDirs = { L".", L"src", L"source", L"sources" };
    }
