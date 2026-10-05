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

