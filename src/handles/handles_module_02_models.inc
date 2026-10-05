// ============================================================================
// 2. Data Model
// ============================================================================
struct ProcessHandleInfo {
    DWORD pid = 0;
    DWORD handleCount = 0;
    std::wstring name;
    bool accessDenied = false;

    // Helper to get narrow UTF-8 representation of the process name
    std::string narrowName() const {
        if (name.empty()) return "";
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, name.c_str(), (int)name.size(), NULL, 0, NULL, NULL);
        std::string strTo(sizeNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, name.c_str(), (int)name.size(), &strTo[0], sizeNeeded, NULL, NULL);
        return strTo;
    }
};

