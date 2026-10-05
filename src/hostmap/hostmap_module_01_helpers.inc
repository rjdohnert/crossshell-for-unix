// ============================================================================
// 1. RAII SCOPES & UTILITIES
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

class ScopedRegistryKey {
public:
    explicit ScopedRegistryKey(HKEY key = nullptr) : m_key(key) {}

    ~ScopedRegistryKey() {
        Close();
    }

    ScopedRegistryKey(const ScopedRegistryKey&) = delete;
    ScopedRegistryKey& operator=(const ScopedRegistryKey&) = delete;

    ScopedRegistryKey(ScopedRegistryKey&& other) noexcept : m_key(other.m_key) {
        other.m_key = nullptr;
    }

    ScopedRegistryKey& operator=(ScopedRegistryKey&& other) noexcept {
        if (this != &other) {
            Close();
            m_key = other.m_key;
            other.m_key = nullptr;
        }
        return *this;
    }

    HKEY Get() const { return m_key; }
    HKEY* Receive() { Close(); return &m_key; }
    bool IsValid() const { return m_key != nullptr; }

    void Close() {
        if (m_key) {
            RegCloseKey(m_key);
            m_key = nullptr;
        }
    }

    static std::string ReadString(HKEY hRoot, const std::string& subKey, const std::string& valueName) {
        ScopedRegistryKey hKey;
        if (RegOpenKeyExA(hRoot, subKey.c_str(), 0, KEY_READ, hKey.Receive()) != ERROR_SUCCESS) {
            return "Unknown";
        }
        char buffer[512] = { 0 };
        DWORD bufferSize = sizeof(buffer);
        DWORD type = 0;
        std::string result = "Unknown";
        if (RegQueryValueExA(hKey.Get(), valueName.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(buffer), &bufferSize) == ERROR_SUCCESS) {
            if (type == REG_SZ || type == REG_EXPAND_SZ) {
                result = std::string(buffer);
            }
        }
        return result;
    }

private:
    HKEY m_key;
};

class FormatUtils {
public:
    static std::string FormatBytes(uint64_t bytes) {
        const char* units[] = { "B", "KB", "MB", "GB", "TB" };
        int unitIndex = 0;
        double count = static_cast<double>(bytes);
        while (count >= 1024.0 && unitIndex < 4) {
            count /= 1024.0;
            unitIndex++;
        }
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << count << " " << units[unitIndex];
        return oss.str();
    }

    static std::string FormatUptime(uint64_t ms) {
        uint64_t seconds = ms / 1000;
        uint64_t days = seconds / 86400;
        seconds %= 86400;
        uint64_t hours = seconds / 3600;
        seconds %= 3600;
        uint64_t minutes = seconds / 60;
        seconds %= 60;

        std::ostringstream oss;
        if (days > 0) oss << days << "d ";
        if (hours > 0 || days > 0) oss << hours << "h ";
        oss << minutes << "m " << seconds << "s";
        return oss.str();
    }

    static std::string ArchitectureToString(WORD arch) {
        switch (arch) {
            case PROCESSOR_ARCHITECTURE_AMD64: return "x86_64 (64-bit AMD/Intel)";
            case PROCESSOR_ARCHITECTURE_ARM:   return "ARM (32-bit)";
            case PROCESSOR_ARCHITECTURE_ARM64: return "ARM64 (64-bit)";
            case PROCESSOR_ARCHITECTURE_INTEL: return "x86 (32-bit)";
            case PROCESSOR_ARCHITECTURE_IA64:  return "Itanium (IA-64)";
            default:                           return "Unknown Architecture";
        }
    }
};

