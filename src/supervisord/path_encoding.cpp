#include "path_encoding.hpp"

std::wstring GetExecutableDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring strPath(path);
    size_t pos = strPath.find_last_of(L"\\/");
    return (pos != std::wstring::npos) ? strPath.substr(0, pos) : L".";
}

std::wstring GetUserHomeDir() {
    wchar_t* userProfile = nullptr;
    size_t len = 0;
    if (_wdupenv_s(&userProfile, &len, L"USERPROFILE") == 0 && userProfile != nullptr) {
        std::wstring home(userProfile);
        free(userProfile);
        return home;
    }
    wchar_t* homeDrive = nullptr;
    wchar_t* homePath = nullptr;
    size_t lenDrive = 0, lenPath = 0;
    _wdupenv_s(&homeDrive, &lenDrive, L"HOMEDRIVE");
    _wdupenv_s(&homePath, &lenPath, L"HOMEPATH");
    if (homeDrive && homePath) {
        std::wstring home = std::wstring(homeDrive) + std::wstring(homePath);
        free(homeDrive);
        free(homePath);
        return home;
    }
    if (homeDrive) free(homeDrive);
    if (homePath) free(homePath);
    return GetExecutableDir();
}

std::string WideToUtf8(const std::wstring& input) {
    if (input.empty()) return std::string();

    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, NULL, 0, NULL, NULL);
    if (sizeNeeded <= 1) return std::string();

    std::string result(static_cast<size_t>(sizeNeeded - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, result.data(), sizeNeeded, NULL, NULL);
    return result;
}

std::wstring Utf8ToWide(const std::string& input) {
    if (input.empty()) return std::wstring();

    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, NULL, 0);
    if (sizeNeeded <= 1) return std::wstring(input.begin(), input.end());

    std::wstring result(static_cast<size_t>(sizeNeeded - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, result.data(), sizeNeeded);
    return result;
}
