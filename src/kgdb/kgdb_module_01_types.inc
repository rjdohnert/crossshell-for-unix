// --- ANSI Escape Code Constants ---
std::string RESET      = "\x1b[0m";
std::string BOLD       = "\x1b[1m";
std::string GREEN      = "\x1b[1;32m";
std::string CYAN       = "\x1b[1;36m";
std::string YELLOW     = "\x1b[1;33m";
std::string RED        = "\x1b[1;31m";
std::string MAGENTA    = "\x1b[1;35m";
std::string GRAY       = "\x1b[90m";

void DisableColor() {
    RESET = "";
    BOLD = "";
    GREEN = "";
    CYAN = "";
    YELLOW = "";
    RED = "";
    MAGENTA = "";
    GRAY = "";
}

bool ParsePortNumber(const std::string& text, int& port) {
    if (text.empty()) return false;
    char* end = nullptr;
    long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0' || parsed < 1 || parsed > 65535) {
        return false;
    }
    port = static_cast<int>(parsed);
    return true;
}

bool ParseHostPort(const std::string& input, std::string& host, int& port, int defaultPort) {
    if (input.empty()) return false;

    std::string value = input;
    if (value.size() >= 2 && value.front() == '[') {
        size_t close = value.find(']');
        if (close == std::string::npos) return false;
        if (close + 1 < value.size() && value[close + 1] == ':') {
            host = value.substr(1, close - 1);
            return ParsePortNumber(value.substr(close + 2), port);
        }
        return false;
    }

    size_t colon = value.rfind(':');
    if (colon == std::string::npos) {
        host = value;
        port = defaultPort;
        return true;
    }

    host = value.substr(0, colon);
    return ParsePortNumber(value.substr(colon + 1), port);
}

class WsaContext {
public:
    WsaContext() {
        initialized = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
    }

    ~WsaContext() {
        if (initialized) {
            WSACleanup();
        }
    }

    bool IsReady() const { return initialized; }

private:
    WSADATA wsaData{};
    bool initialized = false;
};

// --- Windows Kernel Dump Structure ---
#pragma pack(push, 1)
struct DUMP_HEADER64 {
    uint32_t Signature;            // 'PAGE' (0x45474150)
    uint32_t ValidDump;            // 'DU64' (0x34365544)
    uint32_t MajorVersion;
    uint32_t MinorVersion;
    uint64_t DirectoryTableBase;
    uint64_t PfnDataBase;
    uint64_t PsLoadedModuleList;
    uint64_t PsActiveProcessHead;
    uint32_t MachineImageType;     // 0x8664 = x64
    uint32_t NumberProcessors;
    uint32_t BugCheckCode;
    uint64_t BugCheckParameter1;
    uint64_t BugCheckParameter2;
    uint64_t BugCheckParameter3;
    uint64_t BugCheckParameter4;
    char     VersionUser[32];
    uint64_t KdDebuggerDataBlock;
};
#pragma pack(pop)

struct CpuContext64 {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip, rflags;
};

struct KernelModule {
    std::string name;
    uint64_t baseAddress;
    uint32_t size;
};

