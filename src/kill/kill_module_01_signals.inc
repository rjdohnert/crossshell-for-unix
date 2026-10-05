// ============================================================================
// Native NT API Function Pointers (for SIGSTOP / SIGCONT process suspension)
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
// Privilege Manager: Enables SeDebugPrivilege for administrator access
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
// Signal Definitions & Mapping
// ============================================================================
enum class SignalType {
    SIGNULL = 0,   // Check existence
    SIGHUP  = 1,   // Hangup -> Graceful close
    SIGINT  = 2,   // Interrupt -> CTRL+C / Graceful
    SIGQUIT = 3,   // Quit -> CTRL+BREAK / Graceful
    SIGKILL = 9,   // Kill -> Immediate hard termination
    SIGUSR1 = 10,  // User-defined 1
    SIGUSR2 = 12,  // User-defined 2
    SIGTERM = 15,  // Terminate -> Graceful WM_CLOSE fallback
    SIGCONT = 18,  // Continue -> Resume suspended process
    SIGSTOP = 19   // Stop -> Suspend process
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
            { 0,  "SIGNULL", "Signal 0: Check process existence & access", SignalType::SIGNULL },
            { 1,  "SIGHUP",  "Hangup: Send WM_CLOSE to top-level windows",  SignalType::SIGHUP },
            { 2,  "SIGINT",  "Interrupt: Send Ctrl+C or WM_CLOSE",          SignalType::SIGINT },
            { 3,  "SIGQUIT", "Quit: Send Ctrl+Break or WM_CLOSE",          SignalType::SIGQUIT },
            { 9,  "SIGKILL", "Kill: Immediate, uncatchable termination",    SignalType::SIGKILL },
            { 10, "SIGUSR1", "User-defined signal 1",                      SignalType::SIGUSR1 },
            { 12, "SIGUSR2", "User-defined signal 2",                      SignalType::SIGUSR2 },
            { 15, "SIGTERM", "Terminate: Graceful termination request",    SignalType::SIGTERM },
            { 18, "SIGCONT", "Continue: Resume paused process threads",    SignalType::SIGCONT },
            { 19, "SIGSTOP", "Stop: Suspend all process threads",          SignalType::SIGSTOP }
        };
    }

    std::optional<SignalInfo> find(std::string_view query) const {
        std::string upper;
        for (char c : query) upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        if (upper.rfind("SIG", 0) == 0) {
            upper = upper.substr(3);
        }

        // Try numeric match
        try {
            size_t pos = 0;
            int num = std::stoi(upper, &pos);
            if (pos == upper.size()) {
                for (const auto& sig : signals) {
                    if (sig.number == num) return sig;
                }
            }
        } catch (...) {}

        // Try mnemonic match
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

