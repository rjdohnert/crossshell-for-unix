// ============================================================================
// 1. DATA MODELS & RAII SCOPES
// ============================================================================

class WinsockScope {
public:
    WinsockScope() : m_initialized(false) {
        WSADATA wsaData = {};
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            m_initialized = true;
        }
    }

    ~WinsockScope() {
        if (m_initialized) {
            WSACleanup();
        }
    }

    bool IsInitialized() const { return m_initialized; }

private:
    bool m_initialized;
};

class ScopedSocket {
public:
    explicit ScopedSocket(SOCKET sock = INVALID_SOCKET) : m_socket(sock) {}

    ~ScopedSocket() {
        Close();
    }

    ScopedSocket(const ScopedSocket&) = delete;
    ScopedSocket& operator=(const ScopedSocket&) = delete;

    ScopedSocket(ScopedSocket&& other) noexcept : m_socket(other.m_socket) {
        other.m_socket = INVALID_SOCKET;
    }

    ScopedSocket& operator=(ScopedSocket&& other) noexcept {
        if (this != &other) {
            Close();
            m_socket = other.m_socket;
            other.m_socket = INVALID_SOCKET;
        }
        return *this;
    }

    SOCKET Get() const { return m_socket; }
    bool IsValid() const { return m_socket != INVALID_SOCKET; }

    void Close() {
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
    }

    void Reset(SOCKET s = INVALID_SOCKET) {
        Close();
        m_socket = s;
    }

private:
    SOCKET m_socket;
};

enum LogLevel { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR };

class InetdLogger {
public:
    static InetdLogger& Instance() {
        static InetdLogger instance;
        return instance;
    }

    void SetDebug(bool debug) { m_debug = debug; }
    void SetVerbose(bool verbose) { m_verbose = verbose; }
    bool IsDebug() const { return m_debug; }
    bool IsVerbose() const { return m_verbose; }

    void Log(LogLevel level, const std::string& msg) {
        if (level == LOG_DEBUG && !m_debug) return;
        if (level == LOG_INFO && !m_verbose && !m_debug) return;

        std::lock_guard<std::mutex> lock(m_mutex);
        SYSTEMTIME st;
        GetLocalTime(&st);

        const char* level_str = "INFO";
        if (level == LOG_DEBUG) level_str = "DEBUG";
        if (level == LOG_WARN)  level_str = "WARN";
        if (level == LOG_ERROR) level_str = "ERROR";

        char time_buf[32];
        snprintf(time_buf, sizeof(time_buf), "%04d-%02d-%02d %02d:%02d:%02d",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

        std::cout << "[" << time_buf << "] [" << level_str << "] " << msg << std::endl;
    }

private:
    std::mutex m_mutex;
    bool m_debug = false;
    bool m_verbose = false;
};

class AdminPrivilegeChecker {
public:
    static bool IsRunningAsAdmin() {
        BOOL isAdmin = FALSE;
        PSID adminGroup = NULL;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

        if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                     DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
            CheckTokenMembership(NULL, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }
        return isAdmin == TRUE;
    }
};

struct ServiceConfig {
    std::string name;
    std::string sock_type;  // stream / dgram
    std::string protocol;   // tcp / udp
    std::string port_str;   // service name or port number
    bool wait_flag = false; // false = nowait, true = wait
    std::string user;       // Solaris compatibility field
    std::string program;    // Path or "internal"
    std::string args;
    SOCKET listen_sock = INVALID_SOCKET;

    // Shared atomic lock to prevent select() from handling 'wait' sockets while active
    std::shared_ptr<std::atomic<bool>> is_busy = std::make_shared<std::atomic<bool>>(false);
};

