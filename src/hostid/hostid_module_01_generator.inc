// ============================================================================
// 1. RAII SCOPES & HARDWARE IDENTIFIERS
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

class RegistryReader {
public:
    static std::wstring GetMachineGuid() {
        HKEY hKey = nullptr;
        wchar_t guidBuf[256] = { 0 };
        DWORD dwSize = sizeof(guidBuf);

        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
            RegQueryValueExW(hKey, L"MachineGuid", nullptr, nullptr, reinterpret_cast<LPBYTE>(guidBuf), &dwSize);
            RegCloseKey(hKey);
        }
        return std::wstring(guidBuf);
    }
};

class HostIdentifierGenerator {
public:
    static uint32_t HashString32(const std::wstring& str) {
        uint32_t hash = 2166136261u;
        for (wchar_t c : str) {
            hash ^= static_cast<uint32_t>(c);
            hash *= 16777619u;
        }
        return hash;
    }

    static uint32_t ComputeHostId() {
        WinsockScope winsock;
        if (winsock.IsInitialized()) {
            char hostname[256] = { 0 };
            if (gethostname(hostname, sizeof(hostname)) == 0) {
                addrinfo hints = {};
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_STREAM;

                addrinfo* result = nullptr;
                if (getaddrinfo(hostname, nullptr, &hints, &result) == 0 && result != nullptr) {
                    sockaddr_in* sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(result->ai_addr);
                    uint32_t ip = sockaddr_ipv4->sin_addr.s_addr;
                    freeaddrinfo(result);

                    if (ip != 0 && ip != htonl(INADDR_LOOPBACK)) {
                        unsigned char* b = reinterpret_cast<unsigned char*>(&ip);
                        return (static_cast<uint32_t>(b[2]) << 24) |
                               (static_cast<uint32_t>(b[3]) << 16) |
                               (static_cast<uint32_t>(b[0]) << 8)  |
                               (static_cast<uint32_t>(b[1]));
                    }
                }
            }
        }

        std::wstring machineGuid = RegistryReader::GetMachineGuid();
        if (!machineGuid.empty()) {
            return HashString32(machineGuid);
        }

        wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
        DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
        GetComputerNameW(computerName, &size);
        return HashString32(computerName);
    }
};

