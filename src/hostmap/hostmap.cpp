/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include <iostream>
#include <fcntl.h>
#include <io.h>
#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include <chrono>
#include <memory>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "advapi32.lib")

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

// ============================================================================
// 3. HOST TELEMETRY COLLECTOR
// ============================================================================

class HostTelemetryCollector {
public:
    static HostSystemInfo CollectSystemInfo() {
        HostSystemInfo info;

        char compName[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
        DWORD compSize = sizeof(compName);
        if (GetComputerNameA(compName, &compSize)) info.computerName = compName;

        char dnsHost[256] = { 0 };
        DWORD dnsHostSize = sizeof(dnsHost);
        if (GetComputerNameExA(ComputerNameDnsHostname, dnsHost, &dnsHostSize)) info.dnsHostname = dnsHost;

        char dnsDomain[256] = { 0 };
        DWORD dnsDomainSize = sizeof(dnsDomain);
        if (GetComputerNameExA(ComputerNameDnsDomain, dnsDomain, &dnsDomainSize)) {
            info.dnsDomain = (dnsDomainSize > 0 && dnsDomain[0] != '\0') ? dnsDomain : "<None / Workgroup>";
        }

        info.osName = ScopedRegistryKey::ReadString(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName");
        info.osVersion = ScopedRegistryKey::ReadString(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "DisplayVersion");
        info.osBuild = ScopedRegistryKey::ReadString(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "CurrentBuildNumber");

        uint64_t uptimeMs = GetTickCount64();
        info.uptime = FormatUtils::FormatUptime(uptimeMs);

        auto bootTimePoint = std::chrono::system_clock::now() - std::chrono::milliseconds(uptimeMs);
        std::time_t bootTimeT = std::chrono::system_clock::to_time_t(bootTimePoint);
        tm tmBuf;
        localtime_s(&tmBuf, &bootTimeT);
        char timeStr[64];
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tmBuf);
        info.bootTime = timeStr;

        return info;
    }

    static HostCpuInfo CollectCpuInfo() {
        HostCpuInfo info;
        SYSTEM_INFO sysInfo;
        GetNativeSystemInfo(&sysInfo);

        info.architecture = FormatUtils::ArchitectureToString(sysInfo.wProcessorArchitecture);
        info.logicalProcessors = sysInfo.dwNumberOfProcessors;
        info.pageSizeBytes = sysInfo.dwPageSize;
        info.processorModel = ScopedRegistryKey::ReadString(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString");

        DWORD bufferSize = 0;
        GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bufferSize);
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            std::vector<BYTE> buffer(bufferSize);
            if (GetLogicalProcessorInformationEx(RelationProcessorCore, reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()), &bufferSize)) {
                DWORD offset = 0;
                DWORD coreCount = 0;
                while (offset < bufferSize) {
                    auto* pInfo = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data() + offset);
                    if (pInfo->Relationship == RelationProcessorCore) coreCount++;
                    offset += pInfo->Size;
                }
                info.physicalCores = coreCount;
            }
        }
        if (info.physicalCores == 0) info.physicalCores = info.logicalProcessors;

        return info;
    }

    static HostMemoryInfo CollectMemoryInfo() {
        HostMemoryInfo info;
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(MEMORYSTATUSEX);

        if (GlobalMemoryStatusEx(&memStatus)) {
            info.totalPhys = memStatus.ullTotalPhys;
            info.availPhys = memStatus.ullAvailPhys;
            info.usedPhys = memStatus.ullTotalPhys - memStatus.ullAvailPhys;
            info.totalVirtual = memStatus.ullTotalPageFile;
            info.availVirtual = memStatus.ullAvailPageFile;
            info.memoryLoad = memStatus.dwMemoryLoad;
        }
        return info;
    }

    static std::vector<NetworkAdapterInfo> CollectNetworkInfo() {
        std::vector<NetworkAdapterInfo> adapters;
        ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_PREFIX;
        ULONG outBufLen = 15000;
        std::vector<BYTE> buffer(outBufLen);
        PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

        DWORD dwRetVal = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, pAddresses, &outBufLen);
        if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
            buffer.resize(outBufLen);
            pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
            dwRetVal = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, pAddresses, &outBufLen);
        }

        if (dwRetVal == NO_ERROR) {
            for (PIP_ADAPTER_ADDRESSES pCurr = pAddresses; pCurr != nullptr; pCurr = pCurr->Next) {
                if (pCurr->IfType == IF_TYPE_SOFTWARE_LOOPBACK && !pCurr->FirstUnicastAddress) continue;

                NetworkAdapterInfo net;
                net.name = pCurr->AdapterName ? pCurr->AdapterName : "";

                const wchar_t* friendlyName = pCurr->FriendlyName ? pCurr->FriendlyName : L"";
                int required = WideCharToMultiByte(CP_UTF8, 0, friendlyName, -1, nullptr, 0, nullptr, nullptr);
                if (required > 0) {
                    std::vector<char> utf8Buffer(static_cast<size_t>(required));
                    WideCharToMultiByte(CP_UTF8, 0, friendlyName, -1, utf8Buffer.data(), required, nullptr, nullptr);
                    net.description = utf8Buffer.data();
                }

                if (pCurr->PhysicalAddressLength != 0) {
                    std::ostringstream macStream;
                    for (ULONG i = 0; i < pCurr->PhysicalAddressLength; ++i) {
                        if (i != 0) macStream << ":";
                        macStream << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(pCurr->PhysicalAddress[i]);
                    }
                    net.macAddress = macStream.str();
                } else {
                    net.macAddress = "N/A";
                }

                net.status = (pCurr->OperStatus == IfOperStatusUp) ? "UP" : "DOWN";

                for (PIP_ADAPTER_UNICAST_ADDRESS pUni = pCurr->FirstUnicastAddress; pUni != nullptr; pUni = pUni->Next) {
                    char ipStr[INET6_ADDRSTRLEN] = { 0 };
                    sockaddr* sa = pUni->Address.lpSockaddr;
                    if (sa->sa_family == AF_INET) {
                        sockaddr_in* sa_in = reinterpret_cast<sockaddr_in*>(sa);
                        inet_ntop(AF_INET, &(sa_in->sin_addr), ipStr, sizeof(ipStr));
                        net.ipv4Addresses.push_back(ipStr);
                    } else if (sa->sa_family == AF_INET6) {
                        sockaddr_in6* sa_in6 = reinterpret_cast<sockaddr_in6*>(sa);
                        inet_ntop(AF_INET6, &(sa_in6->sin6_addr), ipStr, sizeof(ipStr));
                        net.ipv6Addresses.push_back(ipStr);
                    }
                }

                for (PIP_ADAPTER_GATEWAY_ADDRESS_LH pGw = pCurr->FirstGatewayAddress; pGw != nullptr; pGw = pGw->Next) {
                    char gwStr[INET6_ADDRSTRLEN] = { 0 };
                    sockaddr* sa = pGw->Address.lpSockaddr;
                    if (sa->sa_family == AF_INET) {
                        sockaddr_in* sa_in = reinterpret_cast<sockaddr_in*>(sa);
                        inet_ntop(AF_INET, &(sa_in->sin_addr), gwStr, sizeof(gwStr));
                        net.gateways.push_back(gwStr);
                    }
                }

                for (PIP_ADAPTER_DNS_SERVER_ADDRESS_XP pDns = pCurr->FirstDnsServerAddress; pDns != nullptr; pDns = pDns->Next) {
                    char dnsStr[INET6_ADDRSTRLEN] = { 0 };
                    sockaddr* sa = pDns->Address.lpSockaddr;
                    if (sa->sa_family == AF_INET) {
                        sockaddr_in* sa_in = reinterpret_cast<sockaddr_in*>(sa);
                        inet_ntop(AF_INET, &(sa_in->sin_addr), dnsStr, sizeof(dnsStr));
                        net.dnsServers.push_back(dnsStr);
                    }
                }

                adapters.push_back(net);
            }
        }
        return adapters;
    }
};

// ============================================================================
// 4. OPTIONS & COMMAND LINE PARSER
// ============================================================================

class HostmapOptions {
public:
    bool showSys = false;
    bool showCpu = false;
    bool showMem = false;
    bool showNet = false;
    bool jsonOutput = false;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]) {
        bool explicitSelection = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";
            if (arg == "-h" || arg == "--help" || arg == "/?") {
                showHelp = true;
                return true;
            } else if (arg == "-V" || arg == "--version") {
                showVersion = true;
                return true;
            } else if (arg == "-a" || arg == "--all") {
                showSys = showCpu = showMem = showNet = true;
                explicitSelection = true;
            } else if (arg == "-s" || arg == "--sys") {
                showSys = true;
                explicitSelection = true;
            } else if (arg == "-c" || arg == "--cpu") {
                showCpu = true;
                explicitSelection = true;
            } else if (arg == "-m" || arg == "--mem") {
                showMem = true;
                explicitSelection = true;
            } else if (arg == "-n" || arg == "--net") {
                showNet = true;
                explicitSelection = true;
            } else if (arg == "-j" || arg == "--json") {
                jsonOutput = true;
            } else {
                std::cerr << "hostmap: unknown option '" << arg << "'\n";
                return false;
            }
        }

        if (!explicitSelection) {
            showSys = showCpu = showMem = showNet = true;
        }

        return true;
    }

    void PrintHelp() const {
        std::cout << R"(hostmap(1)              CrossShell for UNIX Reference Manual               hostmap(1)

    NAME
        hostmap - comprehensive system hardware and network topology mapper

    SYNOPSIS
        hostmap [OPTIONS]

    DESCRIPTION
        hostmap queries and renders detailed system configuration, CPU telemetry,
        memory topology, network adapters, and routing maps.

    OPTIONS
        -a, --all
            Display all subsystem topology information.

        -s, --sys
            Display system and motherboard configuration.

        -c, --cpu
            Display CPU architecture and core topology.

        -m, --mem
            Display physical and virtual memory metrics.

        -n, --net
            Display network adapters and interface maps.

        --json, --csv, --table
            Output structured topology records.

        --pipe COMMAND
            Stream results to COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        hostmap -a --json
            Map all hardware subsystems and output as JSON.

    CrossShell for UNIX                                                hostmap(1)
)";
    }

    void PrintVersion() const {
        std::cout << "hostmap 4.9.0\n";
    }
};

// ============================================================================
// 5. OUTPUT REPORTER
// ============================================================================

class HostmapReporter {
public:
    static void PrintSectionHeader(const std::string& title) {
        std::cout << "\n[" << title << "]\n" << std::string(60, '-') << "\n";
    }

    static void PrintKeyValue(const std::string& key, const std::string& value) {
        std::cout << "  " << std::left << std::setw(24) << key << ": " << value << "\n";
    }

    static void RenderText(const HostmapOptions& opts) {
        if (opts.showSys) {
            HostSystemInfo sys = HostTelemetryCollector::CollectSystemInfo();
            PrintSectionHeader("HOST & SYSTEM IDENTITY");
            PrintKeyValue("NetBIOS Name", sys.computerName);
            PrintKeyValue("DNS Hostname", sys.dnsHostname);
            PrintKeyValue("DNS Domain", sys.dnsDomain);
            PrintKeyValue("Operating System", sys.osName + " (Edition " + sys.osVersion + ", Build " + sys.osBuild + ")");
            PrintKeyValue("System Boot Time", sys.bootTime);
            PrintKeyValue("System Uptime", sys.uptime);
        }

        if (opts.showCpu) {
            HostCpuInfo cpu = HostTelemetryCollector::CollectCpuInfo();
            PrintSectionHeader("PROCESSOR & KERNEL TOPOLOGY");
            PrintKeyValue("Processor Model", cpu.processorModel);
            PrintKeyValue("Architecture", cpu.architecture);
            PrintKeyValue("Physical Sockets/Cores", std::to_string(cpu.physicalCores));
            PrintKeyValue("Logical Processors", std::to_string(cpu.logicalProcessors));
            PrintKeyValue("Page Size", std::to_string(cpu.pageSizeBytes) + " bytes");
        }

        if (opts.showMem) {
            HostMemoryInfo mem = HostTelemetryCollector::CollectMemoryInfo();
            PrintSectionHeader("MEMORY & PAGING UTILIZATION");
            PrintKeyValue("Physical RAM Total", FormatUtils::FormatBytes(mem.totalPhys));
            PrintKeyValue("Physical RAM Used", FormatUtils::FormatBytes(mem.usedPhys) + " (" + std::to_string(mem.memoryLoad) + "%)");
            PrintKeyValue("Physical RAM Free", FormatUtils::FormatBytes(mem.availPhys));
            PrintKeyValue("Commit / Paging Total", FormatUtils::FormatBytes(mem.totalVirtual));
            PrintKeyValue("Commit / Paging Free", FormatUtils::FormatBytes(mem.availVirtual));
        }

        if (opts.showNet) {
            auto adapters = HostTelemetryCollector::CollectNetworkInfo();
            PrintSectionHeader("NETWORK INTERFACES & ADDRESS MAP");
            for (const auto& adp : adapters) {
                std::cout << "  * " << adp.description << " [" << adp.status << "]\n";
                std::cout << "      MAC Address : " << adp.macAddress << "\n";
                for (const auto& ip4 : adp.ipv4Addresses) {
                    std::cout << "      IPv4 Address: " << ip4 << "\n";
                }
                for (const auto& ip6 : adp.ipv6Addresses) {
                    std::cout << "      IPv6 Address: " << ip6 << "\n";
                }
                for (const auto& gw : adp.gateways) {
                    std::cout << "      Gateway     : " << gw << "\n";
                }
                for (const auto& dns : adp.dnsServers) {
                    std::cout << "      DNS Server  : " << dns << "\n";
                }
                std::cout << "\n";
            }
        }
    }

    static void RenderJson() {
        HostSystemInfo sys = HostTelemetryCollector::CollectSystemInfo();
        HostCpuInfo cpu = HostTelemetryCollector::CollectCpuInfo();
        HostMemoryInfo mem = HostTelemetryCollector::CollectMemoryInfo();
        auto adapters = HostTelemetryCollector::CollectNetworkInfo();

        std::cout << "{\n"
                  << "  \"system\": {\n"
                  << "    \"netbios_name\": \"" << sys.computerName << "\",\n"
                  << "    \"dns_hostname\": \"" << sys.dnsHostname << "\",\n"
                  << "    \"dns_domain\": \"" << sys.dnsDomain << "\",\n"
                  << "    \"os_name\": \"" << sys.osName << "\",\n"
                  << "    \"os_version\": \"" << sys.osVersion << "\",\n"
                  << "    \"os_build\": \"" << sys.osBuild << "\",\n"
                  << "    \"uptime\": \"" << sys.uptime << "\",\n"
                  << "    \"boot_time\": \"" << sys.bootTime << "\"\n"
                  << "  },\n"
                  << "  \"processor\": {\n"
                  << "    \"model\": \"" << cpu.processorModel << "\",\n"
                  << "    \"architecture\": \"" << cpu.architecture << "\",\n"
                  << "    \"physical_cores\": " << cpu.physicalCores << ",\n"
                  << "    \"logical_processors\": " << cpu.logicalProcessors << ",\n"
                  << "    \"page_size_bytes\": " << cpu.pageSizeBytes << "\n"
                  << "  },\n"
                  << "  \"memory\": {\n"
                  << "    \"total_bytes\": " << mem.totalPhys << ",\n"
                  << "    \"available_bytes\": " << mem.availPhys << ",\n"
                  << "    \"used_bytes\": " << mem.usedPhys << ",\n"
                  << "    \"load_percentage\": " << mem.memoryLoad << ",\n"
                  << "    \"virtual_total_bytes\": " << mem.totalVirtual << ",\n"
                  << "    \"virtual_avail_bytes\": " << mem.availVirtual << "\n"
                  << "  },\n"
                  << "  \"adapters\": [\n";

        for (size_t i = 0; i < adapters.size(); ++i) {
            const auto& adp = adapters[i];
            std::cout << "    {\n"
                      << "      \"description\": \"" << adp.description << "\",\n"
                      << "      \"status\": \"" << adp.status << "\",\n"
                      << "      \"mac_address\": \"" << adp.macAddress << "\",\n"
                      << "      \"ipv4\": [";
            for (size_t j = 0; j < adp.ipv4Addresses.size(); ++j) {
                std::cout << "\"" << adp.ipv4Addresses[j] << "\"" << (j + 1 < adp.ipv4Addresses.size() ? ", " : "");
            }
            std::cout << "],\n      \"ipv6\": [";
            for (size_t j = 0; j < adp.ipv6Addresses.size(); ++j) {
                std::cout << "\"" << adp.ipv6Addresses[j] << "\"" << (j + 1 < adp.ipv6Addresses.size() ? ", " : "");
            }
            std::cout << "]\n    }" << (i + 1 < adapters.size() ? ",\n" : "\n");
        }
        std::cout << "  ]\n}\n";
    }
};

// ============================================================================
// 6. APPLICATION CONTROLLER
// ============================================================================

class HostmapApplication {
public:
    int Run(int argc, char* argv[]) {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        WinsockScope winsock;
        if (!winsock.IsInitialized()) {
            std::cerr << "[-] Error: Failed to initialize Winsock.\n";
            return 1;
        }

        HostmapOptions opts;
        if (!opts.Parse(argc, argv)) {
            std::cerr << "Use 'hostmap --help' for usage.\n";
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintHelp();
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        if (opts.jsonOutput) {
            HostmapReporter::RenderJson();
        } else {
            HostmapReporter::RenderText(opts);
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    HostmapApplication app;
    return app.Run(argc, argv);
}