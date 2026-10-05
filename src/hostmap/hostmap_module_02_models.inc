// ============================================================================
// 2. DATA MODELS
// ============================================================================

struct HostSystemInfo {
    std::string computerName;
    std::string dnsHostname;
    std::string dnsDomain;
    std::string osName;
    std::string osVersion;
    std::string osBuild;
    std::string uptime;
    std::string bootTime;
};

struct HostCpuInfo {
    std::string processorModel;
    std::string architecture;
    DWORD logicalProcessors = 0;
    DWORD physicalCores = 0;
    DWORD pageSizeBytes = 0;
};

struct HostMemoryInfo {
    uint64_t totalPhys = 0;
    uint64_t availPhys = 0;
    uint64_t usedPhys = 0;
    uint64_t totalVirtual = 0;
    uint64_t availVirtual = 0;
    DWORD memoryLoad = 0;
};

struct NetworkAdapterInfo {
    std::string name;
    std::string description;
    std::string macAddress;
    std::string status;
    std::vector<std::string> ipv4Addresses;
    std::vector<std::string> ipv6Addresses;
    std::vector<std::string> gateways;
    std::vector<std::string> dnsServers;
};

