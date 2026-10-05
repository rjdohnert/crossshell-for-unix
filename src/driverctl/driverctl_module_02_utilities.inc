// Converts wide strings to standard UTF-8 strings
std::string ToUtf8(std::wstring_view wstr) {
    if (wstr.empty()) return {};
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    if (sizeNeeded <= 0) return {};
    std::string str(sizeNeeded, 0);
    int written = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), str.data(), sizeNeeded, nullptr, nullptr);
    if (written <= 0) return {};
    str.resize(written);
    return str;
}

// Returns true when the current process token is elevated (UAC-aware).
// Uses TokenElevation rather than the legacy Admin-SID membership test, which
// reports a non-elevated state for split tokens where the Admin SID is
// present but flagged SE_GROUP_USE_FOR_DENY_ONLY.
bool IsRunningElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    bool elevated = false;
    if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
        elevated = (elevation.TokenIsElevated != 0);
    }
    CloseHandle(token);
    return elevated;
}
