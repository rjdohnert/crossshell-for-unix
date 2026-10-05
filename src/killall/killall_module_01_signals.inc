// ============================================================================
// NT API Function Definitions (for SIGSTOP / SIGCONT process freeze/thaw)
// ============================================================================
typedef LONG(NTAPI* PFN_NtSuspendProcess)(HANDLE ProcessHandle);
typedef LONG(NTAPI* PFN_NtResumeProcess)(HANDLE ProcessHandle);

// ============================================================================
// RAII Wrapper for Windows Handles
// ============================================================================
struct HandleDeleter {
    void operator()(HANDLE handle) const {
        if (handle && handle != INVALID_HANDLE_VALUE) {
            ::CloseHandle(handle);
        }
    }
};
using UniqueHandle = std::unique_ptr<void, HandleDeleter>;

// ============================================================================
// Privilege Manager: Escalates to SeDebugPrivilege
// ============================================================================
class PrivilegeManager {
public:
    static bool enableDebugPrivilege() {
        HANDLE hToken = nullptr;
        if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            return false;
        }
        UniqueHandle tokenHandle(hToken);

        TOKEN_PRIVILEGES tp{};
        LUID luid{};
        if (!::LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid)) {
            return false;
        }

        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        return ::AdjustTokenPrivileges(tokenHandle.get(), FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr) &&
               (::GetLastError() != ERROR_NOT_ALL_ASSIGNED);
    }
};

// ============================================================================
// Time / Duration Utilities
// ============================================================================
class TimeHelper {
public:
    static std::optional<std::chrono::seconds> parseDuration(std::string_view str) {
        if (str.empty()) return std::nullopt;
        
        char unit = str.back();
        std::string numPart;
        int64_t multiplier = 1;

        if (std::isalpha(static_cast<unsigned char>(unit))) {
            numPart = str.substr(0, str.size() - 1);
            switch (std::tolower(static_cast<unsigned char>(unit))) {
                case 's': multiplier = 1; break;
                case 'm': multiplier = 60; break;
                case 'h': multiplier = 3600; break;
                case 'd': multiplier = 86400; break;
                case 'w': multiplier = 604800; break;
                default: return std::nullopt;
            }
        } else {
            numPart = str;
            multiplier = 1; // Default to seconds
        }

        try {
            int64_t val = std::stoll(numPart);
            return std::chrono::seconds(val * multiplier);
        } catch (...) {
            return std::nullopt;
        }
    }

    static std::chrono::seconds getProcessAge(FILETIME creationTime) {
        FILETIME nowFt;
        ::GetSystemTimeAsFileTime(&nowFt);

        ULARGE_INTEGER createVal, nowVal;
        createVal.LowPart = creationTime.dwLowDateTime;
        createVal.HighPart = creationTime.dwHighDateTime;
        nowVal.LowPart = nowFt.dwLowDateTime;
        nowVal.HighPart = nowFt.dwHighDateTime;

        if (nowVal.QuadPart < createVal.QuadPart) return std::chrono::seconds(0);
        
        // 100-nanosecond intervals -> seconds
        uint64_t diffSec = (nowVal.QuadPart - createVal.QuadPart) / 10000000ULL;
        return std::chrono::seconds(diffSec);
    }
};

// ============================================================================
// Signal Definitions & Mapping
// ============================================================================
enum class SignalType {
    SIGNULL = 0,
    SIGHUP  = 1,
    SIGINT  = 2,
    SIGQUIT = 3,
    SIGKILL = 9,
    SIGTERM = 15,
    SIGCONT = 18,
    SIGSTOP = 19
};

struct SignalInfo {
    int number;
    std::string name;
    std::string description;
    SignalType type;
};

class SignalRegistry {
private:
    std::vector<SignalInfo> signals;

public:
    SignalRegistry() {
        signals = {
            { 0,  "SIGNULL", "Signal 0: Query process existence and permissions", SignalType::SIGNULL },
            { 1,  "SIGHUP",  "Hangup: Send WM_CLOSE to top-level windows",        SignalType::SIGHUP },
            { 2,  "SIGINT",  "Interrupt: Send Ctrl+C event or WM_CLOSE",          SignalType::SIGINT },
            { 3,  "SIGQUIT", "Quit: Send Ctrl+Break event or WM_CLOSE",          SignalType::SIGQUIT },
            { 9,  "SIGKILL", "Kill: Immediate, uncatchable termination",          SignalType::SIGKILL },
            { 15, "SIGTERM", "Terminate: Graceful close request (with fallback)", SignalType::SIGTERM },
            { 18, "SIGCONT", "Continue: Resume frozen/suspended process threads", SignalType::SIGCONT },
            { 19, "SIGSTOP", "Stop: Suspend all execution threads",               SignalType::SIGSTOP }
        };
    }

    std::optional<SignalInfo> find(std::string_view query) const {
        std::string upper;
        for (char c : query) upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        if (upper.rfind("SIG", 0) == 0) upper = upper.substr(3);

        try {
            size_t pos = 0;
            int num = std::stoi(upper, &pos);
            if (pos == upper.size()) {
                for (const auto& sig : signals) {
                    if (sig.number == num) return sig;
                }
            }
        } catch (...) {}

        for (const auto& sig : signals) {
            std::string sigNameUpper = sig.name;
            if (sigNameUpper.rfind("SIG", 0) == 0) sigNameUpper = sigNameUpper.substr(3);
            if (sigNameUpper == upper || sig.name == upper) {
                return sig;
            }
        }
        return std::nullopt;
    }

    const std::vector<SignalInfo>& getAll() const { return signals; }
};

