// ============================================================================
// DATA MODELS & ENUMS
// ============================================================================
enum class SortField {
    PID,
    USER,
    PRIORITY,
    VIRTUAL_MEM,
    RESIDENT_MEM,
    SHARED_MEM,
    STATE,
    CPU_PERCENT,
    MEM_PERCENT,
    TIME,
    COMMAND
};

enum class ViewMode {
    Normal,
    HelpModal,
    SetupModal,
    SearchInput,
    FilterInput,
    SignalModal,
    UserFilterInput
};

struct CpuCoreData {
    ULONGLONG idleTime = 0;
    ULONGLONG kernelTime = 0;
    ULONGLONG userTime = 0;
    double usagePercent = 0.0;
};

struct ProcessItem {
    DWORD pid = 0;
    DWORD ppid = 0;
    std::wstring name;
    std::wstring fullPath;
    std::wstring user;
    LONG priority = 0;
    SIZE_T virtMem = 0;
    SIZE_T resMem = 0;
    SIZE_T privMem = 0;
    wchar_t state = L'R';
    double cpuPercent = 0.0;
    double memPercent = 0.0;
    ULONGLONG totalCpuTime = 0; // In 100-ns units
    DWORD threadCount = 0;
    bool isTagged = false;
    
    // Parent-child tree structure
    std::vector<DWORD> childPids;
    size_t treeDepth = 0;
    std::wstring treePrefix;
    uint64_t seenGeneration = 0;
};

// User SID Cache to optimize token inspection
static std::unordered_map<DWORD, std::wstring> g_UserCache;
static std::mutex g_UserCacheMutex;

static std::wstring GetProcessOwner(DWORD pid) {
    if (pid == 0 || pid == 4) return L"SYSTEM";

    {
        std::lock_guard<std::mutex> lock(g_UserCacheMutex);
        auto it = g_UserCache.find(pid);
        if (it != g_UserCache.end()) return it->second;
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return L"N/A";

    HANDLE hToken = NULL;
    std::wstring username = L"N/A";

    if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
        DWORD dwSize = 0;
        GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);
        if (dwSize > 0) {
            std::vector<BYTE> buffer(dwSize);
            if (GetTokenInformation(hToken, TokenUser, buffer.data(), dwSize, &dwSize)) {
                PTOKEN_USER pTokenUser = reinterpret_cast<PTOKEN_USER>(buffer.data());
                wchar_t name[256], domain[256];
                DWORD nameLen = 256, domainLen = 256;
                SID_NAME_USE snu;
                if (LookupAccountSidW(NULL, pTokenUser->User.Sid, name, &nameLen, domain, &domainLen, &snu)) {
                    username = std::wstring(domain) + L"\\" + std::wstring(name);
                }
            }
        }
        CloseHandle(hToken);
    }
    CloseHandle(hProcess);

    {
        std::lock_guard<std::mutex> lock(g_UserCacheMutex);
        g_UserCache[pid] = username;
    }

    return username;
}

