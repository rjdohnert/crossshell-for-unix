#include "engine.hpp"

// Trims quotes and whitespace, standardizes slashes to backslashes, removes trailing slashes
std::wstring PthctlEngine::NormalizePath(const std::wstring& path) {
    size_t start = path.find_first_not_of(L" \t\n\r\"");
    if (start == std::wstring::npos) return L"";
    size_t end = path.find_last_not_of(L" \t\n\r\"");
    std::wstring result = path.substr(start, end - start + 1);

    for (auto& ch : result) {
        if (ch == L'/') ch = L'\\';
    }

    // Preserve root slash like "C:\" but remove trailing slash for subdirs ("C:\Tools\" -> "C:\Tools")
    if (result.length() > 3 && result.back() == L'\\') {
        result.pop_back();
    }

    return result;
}

// Case-insensitive comparison suitable for Windows file paths
bool PthctlEngine::PathEquals(const std::wstring& p1, const std::wstring& p2) {
    std::wstring n1 = NormalizePath(p1);
    std::wstring n2 = NormalizePath(p2);
    if (n1.length() != n2.length()) return false;
    return _wcsicmp(n1.c_str(), n2.c_str()) == 0;
}

// Split PATH string by semicolon delimiter
std::vector<std::wstring> PthctlEngine::SplitPath(const std::wstring& rawPath) {
    std::vector<std::wstring> list;
    std::wstringstream ss(rawPath);
    std::wstring item;

    while (std::getline(ss, item, L';')) {
        std::wstring trimmed = NormalizePath(item);
        if (!trimmed.empty()) {
            list.push_back(trimmed);
        }
    }
    return list;
}

// Split PATH string by semicolon delimiter while preserving original entry text
std::vector<std::wstring> PthctlEngine::SplitPathRaw(const std::wstring& rawPath) {
    std::vector<std::wstring> list;
    std::wstringstream ss(rawPath);
    std::wstring item;

    while (std::getline(ss, item, L';')) {
        list.push_back(item);
    }
    return list;
}

// Join vector of paths back into semicolon-delimited string
std::wstring PthctlEngine::JoinPath(const std::vector<std::wstring>& list) {
    std::wstring joined;
    for (size_t i = 0; i < list.size(); ++i) {
        joined += list[i];
        if (i + 1 < list.size()) {
            joined += L";";
        }
    }
    return joined;
}

// Join raw path entries back into a semicolon-delimited string
std::wstring PthctlEngine::JoinRawPath(const std::vector<std::wstring>& list) {
    std::wstring joined;
    for (size_t i = 0; i < list.size(); ++i) {
        joined += list[i];
        if (i + 1 < list.size()) {
            joined += L";";
        }
    }
    return joined;
}

bool PthctlEngine::ValidatePathLength(const std::wstring& updatedPath, bool dryRun) {
    const size_t updatedChars = updatedPath.length();
    if (updatedChars >= PATH_HARD_LIMIT_CHARS) {
        std::wcerr << L"pthctl: error: resulting PATH length (" << updatedChars
                  << L" chars) exceeds Windows limit (" << (PATH_HARD_LIMIT_CHARS - 1)
                  << L" chars).\n";
        return false;
    }

    if (updatedChars > PATH_WARN_LIMIT_CHARS) {
        std::wcerr << L"pthctl: warning: resulting PATH length is " << updatedChars
                  << L" chars; some older tools may fail beyond " << PATH_WARN_LIMIT_CHARS
                  << L" chars.\n";
    }

    return true;
}

// Reads PATH variable from Registry
bool PthctlEngine::ReadRegistryPath(Scope scope, std::wstring& outPath, DWORD& outType) {
    HKEY hKey;
    HKEY rootKey = (scope == Scope::User) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    const wchar_t* subKey = (scope == Scope::User) ? USER_ENV_KEY : SYSTEM_ENV_KEY;

    LONG status = RegOpenKeyExW(rootKey, subKey, 0, KEY_READ, &hKey);
    if (status != ERROR_SUCCESS) {
        std::wcerr << L"pthctl: error opening registry key for reading (code " << status << L")\n";
        return false;
    }

    DWORD bytesNeeded = 0;
    DWORD type = 0;
    
    // Try reading "Path" or "PATH"
    status = RegQueryValueExW(hKey, L"Path", NULL, &type, NULL, &bytesNeeded);
    const wchar_t* valueName = L"Path";
    if (status != ERROR_SUCCESS) {
        status = RegQueryValueExW(hKey, L"PATH", NULL, &type, NULL, &bytesNeeded);
        valueName = L"PATH";
    }

    if (status != ERROR_SUCCESS && status != ERROR_MORE_DATA) {
        RegCloseKey(hKey);
        outPath = L"";
        outType = REG_EXPAND_SZ;
        return true; // Key exists, but PATH string is empty
    }

    std::vector<wchar_t> buffer((bytesNeeded / sizeof(wchar_t)) + 1, 0);
    status = RegQueryValueExW(hKey, valueName, NULL, &type, reinterpret_cast<LPBYTE>(buffer.data()), &bytesNeeded);
    RegCloseKey(hKey);

    if (status == ERROR_SUCCESS) {
        outPath = buffer.data();
        outType = type;
        return true;
    }

    return false;
}

// Writes updated PATH to Registry and notifies running applications
bool PthctlEngine::WriteRegistryPath(Scope scope, const std::wstring& newPath, DWORD type) {
    HKEY hKey;
    HKEY rootKey = (scope == Scope::User) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    const wchar_t* subKey = (scope == Scope::User) ? USER_ENV_KEY : SYSTEM_ENV_KEY;

    LONG status = RegOpenKeyExW(rootKey, subKey, 0, KEY_SET_VALUE, &hKey);
    if (status != ERROR_SUCCESS) {
        if (status == ERROR_ACCESS_DENIED) {
            std::wcerr << L"pthctl: error: access denied. Modifying "
                       << (scope == Scope::User ? L"user" : L"system")
                       << L" PATH requires elevated (Administrator) privileges.\n";
        } else {
            std::wcerr << L"pthctl: error opening registry key for writing (code " << status << L")\n";
        }
        return false;
    }

    // Default to REG_EXPAND_SZ for environment paths
    if (type != REG_SZ && type != REG_EXPAND_SZ) {
        type = REG_EXPAND_SZ;
    }

    size_t byteCount = (newPath.length() + 1) * sizeof(wchar_t);
    status = RegSetValueExW(hKey, L"Path", 0, type,
                            reinterpret_cast<const BYTE*>(newPath.c_str()),
                            static_cast<DWORD>(byteCount));
    RegCloseKey(hKey);

    if (status != ERROR_SUCCESS) {
        std::wcerr << L"pthctl: error writing to registry (code " << status << L")\n";
        return false;
    }

    // Notify top-level windows of environment variable changes
    DWORD_PTR dwResult = 0;
    if (!SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                             reinterpret_cast<LPARAM>(L"Environment"),
                             SMTO_ABORTIFHUNG, 5000, &dwResult)) {
        DWORD notifyError = GetLastError();
        std::wcerr << L"pthctl: warning: PATH updated, but environment change notification failed";
        if (notifyError != ERROR_SUCCESS) {
            std::wcerr << L" (code " << notifyError << L")";
        }
        std::wcerr << L". New shells may be required to see the change.\n";
    }

    return true;
}

int PthctlEngine::CommandList(Scope scope) {
    std::wstring rawPath;
    DWORD type = 0;
    if (!ReadRegistryPath(scope, rawPath, type)) return 1;

    auto entries = SplitPath(rawPath);
    std::wcout << L"[" << (scope == Scope::User ? L"User" : L"System") << L" PATH (" << entries.size() << L" entries)]\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        std::wcout << L"  " << (i + 1) << L". " << entries[i] << L"\n";
    }
    return 0;
}

int PthctlEngine::CommandAdd(Scope scope, const std::wstring& targetPath, bool prepend, bool dryRun) {
    std::wstring norm = NormalizePath(targetPath);
    if (norm.empty()) {
        std::wcerr << L"pthctl: error: invalid or empty path specified.\n";
        return 1;
    }

    std::wstring rawPath;
    DWORD type = 0;
    if (!ReadRegistryPath(scope, rawPath, type)) return 1;

    auto entries = SplitPathRaw(rawPath);

    for (const auto& entry : entries) {
        if (PathEquals(entry, norm)) {
            std::wcout << L"pthctl: path is already in " 
                      << (scope == Scope::User ? L"User" : L"System") 
                      << L" PATH: " << norm << L"\n";
            return 0;
        }
    }

    if (prepend) {
        entries.insert(entries.begin(), norm);
    } else {
        entries.push_back(norm);
    }

    std::wstring updatedPath = JoinRawPath(entries);
    if (!ValidatePathLength(updatedPath, dryRun)) {
        return 1;
    }

    if (dryRun) {
        std::wcout << L"pthctl: dry-run: would add '" << norm << L"' to "
                  << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
        return 0;
    }

    if (WriteRegistryPath(scope, updatedPath, type)) {
        std::wcout << L"pthctl: successfully added '" << norm << L"' to " 
                  << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
        return 0;
    }

    return 1;
}

int PthctlEngine::CommandRemove(Scope scope, const std::wstring& targetPath, bool dryRun) {
    std::wstring norm = NormalizePath(targetPath);
    if (norm.empty()) {
        std::wcerr << L"pthctl: error: invalid or empty path specified.\n";
        return 1;
    }

    std::wstring rawPath;
    DWORD type = 0;
    if (!ReadRegistryPath(scope, rawPath, type)) return 1;

    auto entries = SplitPathRaw(rawPath);
    size_t initialSize = entries.size();

    entries.erase(
        std::remove_if(entries.begin(), entries.end(), [&](const std::wstring& e) {
            return PathEquals(e, norm);
        }),
        entries.end()
    );

    if (entries.size() == initialSize) {
        std::wcout << L"pthctl: path not found in " 
                  << (scope == Scope::User ? L"User" : L"System") 
                  << L" PATH: " << norm << L"\n";
        return 0;
    }

    std::wstring updatedPath = JoinRawPath(entries);
    if (!ValidatePathLength(updatedPath, dryRun)) {
        return 1;
    }

    if (dryRun) {
        std::wcout << L"pthctl: dry-run: would remove '" << norm << L"' from "
                  << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
        return 0;
    }

    if (WriteRegistryPath(scope, updatedPath, type)) {
        std::wcout << L"pthctl: successfully removed '" << norm << L"' from " 
                  << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
        return 0;
    }

    return 1;
}

int PthctlEngine::CommandCheck(Scope scope, const std::wstring& targetPath) {
    std::wstring norm = NormalizePath(targetPath);
    if (norm.empty()) {
        std::wcerr << L"pthctl: error: invalid path specified.\n";
        return 1;
    }

    std::wstring rawPath;
    DWORD type = 0;
    if (!ReadRegistryPath(scope, rawPath, type)) return 1;

    auto entries = SplitPath(rawPath);
    for (size_t i = 0; i < entries.size(); ++i) {
        if (PathEquals(entries[i], norm)) {
            std::wcout << L"FOUND: '" << norm << L"' at position " << (i + 1) << L" in "
                      << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
            return 0;
        }
    }

    std::wcout << L"NOT FOUND: '" << norm << L"' is not in "
              << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
    return 1;
}

int PthctlEngine::CommandClean(Scope scope, bool dryRun) {
    std::wstring rawPath;
    DWORD type = 0;
    if (!ReadRegistryPath(scope, rawPath, type)) return 1;

    auto entries = SplitPathRaw(rawPath);
    std::vector<std::wstring> cleanedRaw;
    std::vector<std::wstring> cleanedNormalized;

    size_t entriesRemoved = 0;

    for (const auto& entry : entries) {
        std::wstring normalized = NormalizePath(entry);
        if (normalized.empty()) {
            entriesRemoved++;
            continue;
        }

        bool duplicate = false;
        for (const auto& existing : cleanedNormalized) {
            if (PathEquals(existing, normalized)) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate) {
            cleanedRaw.push_back(entry);
            cleanedNormalized.push_back(normalized);
        } else {
            entriesRemoved++;
        }
    }

    if (entriesRemoved == 0) {
        std::wcout << L"pthctl: no duplicate or empty entries found in "
                  << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
        return 0;
    }

    std::wstring updatedPath = JoinRawPath(cleanedRaw);
    if (!ValidatePathLength(updatedPath, dryRun)) {
        return 1;
    }

    if (dryRun) {
        std::wcout << L"pthctl: dry-run: would clean up " << entriesRemoved
                  << L" duplicate/empty entry/entries from "
                  << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
        return 0;
    }

    if (WriteRegistryPath(scope, updatedPath, type)) {
        std::wcout << L"pthctl: cleaned up " << entriesRemoved << L" duplicate/empty entry/entries from "
                  << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
        return 0;
    }

    return 1;
}
