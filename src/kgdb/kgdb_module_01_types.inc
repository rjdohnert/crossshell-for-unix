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

#pragma comment(lib, "advapi32.lib")

// Enable SeDebugPrivilege so we can query kernel module lists and system memory
bool EnableDebugPrivilege() {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }
    TOKEN_PRIVILEGES tp{};
    LUID luid{};
    bool acquired = false;
    if (LookupPrivilegeValueA(NULL, "SeDebugPrivilege", &luid)) {
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        SetLastError(ERROR_SUCCESS);
        if (AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL)) {
            acquired = (GetLastError() == ERROR_SUCCESS);
        }
    }
    CloseHandle(hToken);
    return acquired;
}

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

// --- Windows Minidump Format Structures ---
struct MINIDUMP_LOCATION_DESCRIPTOR_LITE {
    uint32_t DataSize;
    uint32_t Rva;
};

struct MINIDUMP_HEADER_LITE {
    uint32_t Signature;            // 'MDMP' (0x504d444d)
    uint32_t Version;
    uint32_t NumberOfStreams;
    uint32_t StreamDirectoryRva;
    uint32_t CheckSum;
    uint32_t TimeDateStamp;
    uint64_t Flags;
};

struct MINIDUMP_DIRECTORY_LITE {
    uint32_t StreamType;
    MINIDUMP_LOCATION_DESCRIPTOR_LITE Location;
};

struct MINIDUMP_MODULE_LITE {
    uint64_t BaseOfImage;
    uint32_t SizeOfImage;
    uint32_t CheckSum;
    uint32_t TimeDateStamp;
    uint32_t ModuleNameRva;
    uint8_t  VersionInfo[52];
    MINIDUMP_LOCATION_DESCRIPTOR_LITE CvRecord;
    MINIDUMP_LOCATION_DESCRIPTOR_LITE MiscRecord;
    uint64_t Reserved0;
    uint64_t Reserved1;
};

struct MINIDUMP_EXCEPTION_STREAM_LITE {
    uint32_t ThreadId;
    uint32_t Alignment;
    uint32_t ExceptionCode;
    uint32_t ExceptionFlags;
    uint64_t ExceptionRecord;
    uint64_t ExceptionAddress;
    uint32_t NumberParameters;
    uint32_t UnusedAlignment;
    uint64_t ExceptionInformation[15];
    MINIDUMP_LOCATION_DESCRIPTOR_LITE ThreadContext;
};

struct MINIDUMP_SYSTEM_INFO_LITE {
    uint16_t ProcessorArchitecture; // 9 = PROCESSOR_ARCHITECTURE_AMD64, 12 = ARM64, 0 = x86
    uint16_t ProcessorLevel;
    uint16_t ProcessorRevision;
    uint8_t  NumberOfProcessors;
    uint8_t  ProductType;
    uint32_t MajorVersion;
    uint32_t MinorVersion;
    uint32_t BuildNumber;
    uint32_t PlatformId;
    uint32_t CsdVersionRva;
    uint16_t SuiteMask;
    uint16_t Reserved2;
    uint8_t  CpuInformation[24];
};
#pragma pack(pop)

struct CpuContext64 {
    uint64_t rax = 0, rbx = 0, rcx = 0, rdx = 0;
    uint64_t rsi = 0, rdi = 0, rbp = 0, rsp = 0;
    uint64_t r8  = 0, r9  = 0, r10 = 0, r11 = 0;
    uint64_t r12 = 0, r13 = 0, r14 = 0, r15 = 0;
    uint64_t rip = 0, rflags = 0;
    uint16_t cs = 0, ss = 0, ds = 0, es = 0, fs = 0, gs = 0;
    bool valid = false;
};

struct KernelModule {
    std::string name;
    uint64_t baseAddress;
    uint32_t size;
    std::string path;
};

// --- Native NT Structure for Module Information ---
typedef LONG NTSTATUS;

typedef struct _RTL_PROCESS_MODULE_INFORMATION {
    HANDLE Section;
    PVOID MappedBase;
    PVOID ImageBase;
    ULONG ImageSize;
    ULONG Flags;
    USHORT LoadOrderIndex;
    USHORT InitOrderIndex;
    USHORT LoadCount;
    USHORT OffsetToFileName;
    UCHAR FullPathName[256];
} RTL_PROCESS_MODULE_INFORMATION, *PRTL_PROCESS_MODULE_INFORMATION;

typedef struct _RTL_PROCESS_MODULES {
    ULONG NumberOfModules;
    RTL_PROCESS_MODULE_INFORMATION Modules[1];
} RTL_PROCESS_MODULES, *PRTL_PROCESS_MODULES;

typedef NTSTATUS(NTAPI* pfnNtQuerySystemInformation)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);
