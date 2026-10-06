#include "engine.hpp"

namespace Netmap {

std::string FormatWin32Error(DWORD errorCode) {
    if (errorCode == ERROR_SUCCESS) return "Success";
    LPSTR buffer = nullptr;
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&buffer, 0, nullptr
    );
    std::string message = (size > 0 && buffer) ? buffer : "Unknown error (" + std::to_string(errorCode) + ")";
    if (buffer) LocalFree(buffer);
    while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == ' ')) {
        message.pop_back();
    }
    return message;
}

std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), &str[0], size, nullptr, nullptr);
    return str;
}

bool IsUserAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

std::string FormatMac(const uint8_t* mac, ULONG len) {
    if (!mac || len == 0) return "---";
    std::ostringstream oss;
    for (ULONG i = 0; i < len; ++i) {
        if (i > 0) oss << "-";
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(mac[i]);
    }
    return oss.str();
}

bool ParseMac(const std::string& str, uint8_t mac[6]) {
    std::string cleaned;
    for (char c : str) {
        if (c != '-' && c != ':' && c != '.') cleaned += c;
    }
    if (cleaned.length() != 12) return false;

    for (int i = 0; i < 6; ++i) {
        std::string byteStr = cleaned.substr(i * 2, 2);
        char* end = nullptr;
        long val = std::strtol(byteStr.c_str(), &end, 16);
        if (*end != '\0' || val < 0 || val > 255) return false;
        mac[i] = static_cast<uint8_t>(val);
    }
    return true;
}

bool IsValidIpv4(const std::string& ip) {
    IN_ADDR addr;
    return InetPtonA(AF_INET, ip.c_str(), &addr) == 1;
}

std::string StateToString(NL_NEIGHBOR_STATE state) {
    switch (state) {
        case NlnsUnreachable: return "Unreachable";
        case NlnsIncomplete:  return "Incomplete";
        case NlnsProbe:       return "Probe";
        case NlnsDelay:       return "Delay";
        case NlnsStale:       return "Stale";
        case NlnsReachable:   return "Reachable";
        case NlnsPermanent:   return "Permanent";
        default:              return "Unknown";
    }
}

// --- InterfaceManager Implementation ---
std::vector<InterfaceInfo> InterfaceManager::GetAllInterfaces() {
    std::vector<InterfaceInfo> result;
    ULONG bufferSize = 15000;
    std::vector<BYTE> buffer(bufferSize);
    PIP_ADAPTER_ADDRESSES adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());

    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS;
    ULONG ret = GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &bufferSize);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(bufferSize);
        adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        ret = GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &bufferSize);
    }

    if (ret != NO_ERROR) return result;

    for (PIP_ADAPTER_ADDRESSES curr = adapters; curr != nullptr; curr = curr->Next) {
        InterfaceInfo info;
        info.index = curr->IfIndex;
        info.luid = curr->Luid;
        info.name = curr->AdapterName ? curr->AdapterName : "";
        info.status = curr->OperStatus;
        info.mtu = curr->Mtu;

        if (curr->FriendlyName) {
            info.friendlyName = WideToUtf8(curr->FriendlyName);
        }
        if (curr->Description) {
            info.description = WideToUtf8(curr->Description);
        }

        info.macAddress = FormatMac(curr->PhysicalAddress, curr->PhysicalAddressLength);

        for (PIP_ADAPTER_UNICAST_ADDRESS unicast = curr->FirstUnicastAddress; unicast; unicast = unicast->Next) {
            if (unicast->Address.lpSockaddr->sa_family == AF_INET) {
                sockaddr_in* sa_in = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);
                char ipBuffer[INET_ADDRSTRLEN];
                if (InetNtopA(AF_INET, &(sa_in->sin_addr), ipBuffer, sizeof(ipBuffer))) {
                    info.ipv4Addresses.push_back(ipBuffer);
                }
            }
        }
        result.push_back(info);
    }
    return result;
}

std::optional<InterfaceInfo> InterfaceManager::FindByIpOrIndex(const std::string& identifier) {
    auto ifaces = GetAllInterfaces();
    for (const auto& iface : ifaces) {
        if (std::to_string(iface.index) == identifier) return iface;
        for (const auto& ip : iface.ipv4Addresses) {
            if (ip == identifier) return iface;
        }
        if (iface.friendlyName == identifier) return iface;
    }
    return std::nullopt;
}

std::optional<InterfaceInfo> InterfaceManager::ResolveBestInterfaceForIp(const std::string& targetIp) {
    IN_ADDR destAddr;
    if (InetPtonA(AF_INET, targetIp.c_str(), &destAddr) != 1) return std::nullopt;

    DWORD bestIfIndex = 0;
    if (GetBestInterface(destAddr.S_un.S_addr, &bestIfIndex) == NO_ERROR) {
        auto ifaces = GetAllInterfaces();
        for (const auto& iface : ifaces) {
            if (iface.index == bestIfIndex) return iface;
        }
    }
    return std::nullopt;
}

// --- ArpEngine Implementation ---
bool ArpEngine::DisplayTable(const std::optional<std::string>& ipFilter, 
                             const std::optional<std::string>& ifFilter,
                             bool verbose) 
{
    PMIB_IPNET_TABLE2 table = nullptr;
    DWORD status = GetIpNetTable2(AF_INET, &table);
    if (status != NO_ERROR) {
        std::cerr << "[-] Failed to retrieve ARP table: " << FormatWin32Error(status) << "\n";
        return false;
    }

    auto ifaces = InterfaceManager::GetAllInterfaces();
    std::map<NET_IFINDEX, InterfaceInfo> ifaceMap;
    for (const auto& iface : ifaces) {
        ifaceMap[iface.index] = iface;
    }

    std::map<NET_IFINDEX, std::vector<MIB_IPNET_ROW2>> groupedRows;
    for (ULONG i = 0; i < table->NumEntries; ++i) {
        const auto& row = table->Table[i];
        
        char ipStr[INET_ADDRSTRLEN] = {0};
        InetNtopA(AF_INET, &(row.Address.Ipv4.sin_addr), ipStr, sizeof(ipStr));

        if (ipFilter && *ipFilter != ipStr) continue;

        if (ifFilter) {
            auto matchedIf = InterfaceManager::FindByIpOrIndex(*ifFilter);
            if (!matchedIf || matchedIf->index != row.InterfaceIndex) continue;
        }

        if (!verbose && row.State == NlnsUnreachable) continue;

        groupedRows[row.InterfaceIndex].push_back(row);
    }

    if (groupedRows.empty()) {
        std::cout << "No matching ARP entries found.\n";
        FreeMibTable(table);
        return true;
    }

    for (const auto& [ifIndex, rows] : groupedRows) {
        std::string ifIp = "Unknown";
        std::string ifName = "Interface " + std::to_string(ifIndex);
        if (ifaceMap.count(ifIndex)) {
            const auto& info = ifaceMap[ifIndex];
            if (!info.ipv4Addresses.empty()) ifIp = info.ipv4Addresses[0];
            if (!info.friendlyName.empty()) ifName = info.friendlyName;
        }

        std::cout << "\nInterface: " << ifIp << " --- 0x" << std::hex << ifIndex << std::dec;
        std::cout << " [" << ifName << "]\n";
        std::cout << "  " << std::left << std::setw(22) << "Internet Address"
                  << std::left << std::setw(22) << "Physical Address"
                  << std::left << std::setw(14) << "Type"
                  << std::left << std::setw(14) << "State" << "\n";
        std::cout << "  " << std::string(70, '-') << "\n";

        for (const auto& row : rows) {
            char ipStr[INET_ADDRSTRLEN] = {0};
            InetNtopA(AF_INET, &(row.Address.Ipv4.sin_addr), ipStr, sizeof(ipStr));

            std::string macStr = FormatMac(row.PhysicalAddress, row.PhysicalAddressLength);
            std::string typeStr = (row.State == NlnsPermanent) ? "static" : "dynamic";
            std::string stateStr = StateToString(row.State);

            std::cout << "  " << std::left << std::setw(22) << ipStr
                      << std::left << std::setw(22) << macStr
                      << std::left << std::setw(14) << typeStr
                      << std::left << std::setw(14) << stateStr << "\n";
        }
    }

    FreeMibTable(table);
    return true;
}

bool ArpEngine::AddEntry(const std::string& ipAddress, const std::string& macAddress, 
                         const std::optional<std::string>& ifIdentifier) 
{
    if (!IsUserAdmin()) {
        std::cerr << "[-] Error: Administrative privileges are required to add static ARP entries.\n"
                  << "    Please run netmap from an elevated Command Prompt or PowerShell.\n";
        return false;
    }

    uint8_t mac[6];
    if (!ParseMac(macAddress, mac)) {
        std::cerr << "[-] Invalid MAC address format: " << macAddress << "\n"
                  << "    Valid formats: aa-bb-cc-dd-ee-ff, aa:bb:cc:dd:ee:ff, aabbccddeeff\n";
        return false;
    }

    std::optional<InterfaceInfo> targetIf;
    if (ifIdentifier) {
        targetIf = InterfaceManager::FindByIpOrIndex(*ifIdentifier);
        if (!targetIf) {
            std::cerr << "[-] Interface not found matching: " << *ifIdentifier << "\n";
            return false;
        }
    } else {
        targetIf = InterfaceManager::ResolveBestInterfaceForIp(ipAddress);
        if (!targetIf) {
            std::cerr << "[-] Could not automatically determine the outgoing interface for " << ipAddress << ".\n"
                      << "    Specify the interface explicitly using -N <if_ip_or_index>.\n";
            return false;
        }
    }

    MIB_IPNET_ROW2 row;
    memset(&row, 0, sizeof(row));
    row.InterfaceIndex = targetIf->index;
    row.InterfaceLuid = targetIf->luid;
    row.Address.si_family = AF_INET;
    InetPtonA(AF_INET, ipAddress.c_str(), &(row.Address.Ipv4.sin_addr));

    memcpy(row.PhysicalAddress, mac, 6);
    row.PhysicalAddressLength = 6;
    row.State = NlnsPermanent;

    DWORD status = CreateIpNetEntry2(&row);
    if (status == ERROR_OBJECT_ALREADY_EXISTS) {
        status = SetIpNetEntry2(&row);
    }

    if (status != NO_ERROR) {
        std::cerr << "[-] Failed to set ARP entry: " << FormatWin32Error(status) << "\n";
        return false;
    }

    std::cout << "[+] Successfully added static mapping: " << ipAddress 
              << " -> " << FormatMac(mac, 6) 
              << " on interface " << targetIf->index 
              << " (" << (!targetIf->ipv4Addresses.empty() ? targetIf->ipv4Addresses[0] : targetIf->friendlyName) << ")\n";
    return true;
}

bool ArpEngine::DeleteEntry(const std::string& ipAddress, const std::optional<std::string>& ifIdentifier) {
    if (!IsUserAdmin()) {
        std::cerr << "[-] Error: Administrative privileges are required to delete ARP entries.\n"
                  << "    Please run netmap from an elevated Command Prompt or PowerShell.\n";
        return false;
    }

    if (ipAddress == "*") {
        return FlushEntries(ifIdentifier);
    }

    std::vector<InterfaceInfo> targetIfs;
    if (ifIdentifier) {
        auto matched = InterfaceManager::FindByIpOrIndex(*ifIdentifier);
        if (!matched) {
            std::cerr << "[-] Specified interface not found: " << *ifIdentifier << "\n";
            return false;
        }
        targetIfs.push_back(*matched);
    } else {
        targetIfs = InterfaceManager::GetAllInterfaces();
    }

    bool deletedAny = false;
    for (const auto& iface : targetIfs) {
        MIB_IPNET_ROW2 row;
        memset(&row, 0, sizeof(row));
        row.InterfaceIndex = iface.index;
        row.InterfaceLuid = iface.luid;
        row.Address.si_family = AF_INET;
        InetPtonA(AF_INET, ipAddress.c_str(), &(row.Address.Ipv4.sin_addr));

        DWORD status = DeleteIpNetEntry2(&row);
        if (status == NO_ERROR) {
            std::cout << "[+] Deleted entry for " << ipAddress << " on interface " << iface.index << "\n";
            deletedAny = true;
        }
    }

    if (!deletedAny) {
        std::cerr << "[-] The specified ARP entry for " << ipAddress << " was not found.\n";
        return false;
    }
    return true;
}

bool ArpEngine::FlushEntries(const std::optional<std::string>& ifIdentifier) {
    if (!IsUserAdmin()) {
        std::cerr << "[-] Error: Administrative privileges are required to flush the ARP cache.\n";
        return false;
    }

    NET_IFINDEX ifIndex = 0;
    if (ifIdentifier) {
        auto iface = InterfaceManager::FindByIpOrIndex(*ifIdentifier);
        if (!iface) {
            std::cerr << "[-] Interface not found: " << *ifIdentifier << "\n";
            return false;
        }
        ifIndex = iface->index;
    }

    DWORD status = FlushIpNetTable2(AF_INET, ifIndex);
    if (status != NO_ERROR) {
        std::cerr << "[-] Failed to flush ARP table: " << FormatWin32Error(status) << "\n";
        return false;
    }

    if (ifIndex != 0) {
        std::cout << "[+] Successfully flushed ARP cache for interface index " << ifIndex << ".\n";
    } else {
        std::cout << "[+] Successfully flushed ARP cache for all interfaces.\n";
    }
    return true;
}

bool ArpEngine::ScanSubnet(const std::string& cidrOrRange, int timeoutMs, int threadCount) {
    (void)timeoutMs;
    std::vector<uint32_t> targetIps;

    if (cidrOrRange.find('/') != std::string::npos) {
        size_t slashPos = cidrOrRange.find('/');
        std::string baseIp = cidrOrRange.substr(0, slashPos);
        int prefix = std::stoi(cidrOrRange.substr(slashPos + 1));
        if (prefix < 16 || prefix > 30) {
            std::cerr << "[-] Subnet mask out of recommended range (/16 to /30).\n";
            return false;
        }

        IN_ADDR inAddr;
        if (InetPtonA(AF_INET, baseIp.c_str(), &inAddr) != 1) {
            std::cerr << "[-] Invalid base IP in CIDR: " << baseIp << "\n";
            return false;
        }

        uint32_t hostIp = ntohl(inAddr.S_un.S_addr);
        uint32_t mask = ~((1ULL << (32 - prefix)) - 1);
        uint32_t netAddr = hostIp & mask;
        uint32_t broadcast = netAddr | ~mask;

        for (uint32_t ip = netAddr + 1; ip < broadcast; ++ip) {
            targetIps.push_back(ip);
        }
    } else if (cidrOrRange.find('-') != std::string::npos) {
        size_t dashPos = cidrOrRange.find('-');
        std::string startIp = cidrOrRange.substr(0, dashPos);
        std::string endIp = cidrOrRange.substr(dashPos + 1);

        IN_ADDR startAddr, endAddr;
        if (InetPtonA(AF_INET, startIp.c_str(), &startAddr) != 1 ||
            InetPtonA(AF_INET, endIp.c_str(), &endAddr) != 1) {
            std::cerr << "[-] Invalid IP range specified.\n";
            return false;
        }

        uint32_t start = ntohl(startAddr.S_un.S_addr);
        uint32_t end = ntohl(endAddr.S_un.S_addr);
        if (start > end || (end - start) > 65535) {
            std::cerr << "[-] Range invalid or too large (max 65535 hosts).\n";
            return false;
        }

        for (uint32_t ip = start; ip <= end; ++ip) {
            targetIps.push_back(ip);
        }
    } else {
        IN_ADDR addr;
        if (InetPtonA(AF_INET, cidrOrRange.c_str(), &addr) != 1) {
            std::cerr << "[-] Invalid target IP or CIDR: " << cidrOrRange << "\n";
            return false;
        }
        targetIps.push_back(ntohl(addr.S_un.S_addr));
    }

    std::cout << "[*] Initiating active ARP sweep across " << targetIps.size() << " host(s)...\n";
    std::cout << "  " << std::left << std::setw(20) << "IP Address" 
              << std::left << std::setw(22) << "Physical Address" 
              << std::left << std::setw(15) << "Latency" << "\n";
    std::cout << "  " << std::string(57, '-') << "\n";

    std::mutex coutMutex;
    std::atomic<size_t> ipIndex{0};
    std::atomic<size_t> aliveCount{0};

    auto worker = [&]() {
        while (true) {
            size_t idx = ipIndex.fetch_add(1);
            if (idx >= targetIps.size()) break;

            uint32_t ipHostOrder = targetIps[idx];
            IPAddr destIp = htonl(ipHostOrder);

            ULONG macAddr[2] = {0};
            ULONG macLen = 6;

            auto start = std::chrono::high_resolution_clock::now();
            DWORD ret = SendARP(destIp, 0, macAddr, &macLen);
            auto end = std::chrono::high_resolution_clock::now();

            if (ret == NO_ERROR && macLen == 6) {
                aliveCount++;
                auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
                std::string latencyStr;
                if (durationUs < 1000) {
                    latencyStr = std::to_string(durationUs) + " us";
                } else {
                    latencyStr = std::to_string(durationUs / 1000.0).substr(0, 4) + " ms";
                }

                IN_ADDR outAddr;
                outAddr.S_un.S_addr = destIp;
                char ipBuffer[INET_ADDRSTRLEN];
                InetNtopA(AF_INET, &outAddr, ipBuffer, sizeof(ipBuffer));

                std::string macStr = FormatMac(reinterpret_cast<uint8_t*>(macAddr), 6);

                std::lock_guard<std::mutex> lock(coutMutex);
                std::cout << "  " << std::left << std::setw(20) << ipBuffer
                          << std::left << std::setw(22) << macStr
                          << std::left << std::setw(15) << latencyStr << "\n";
            }
        }
    };

    std::vector<std::future<void>> futures;
    for (int i = 0; i < threadCount; ++i) {
        futures.push_back(std::async(std::launch::async, worker));
    }

    for (auto& f : futures) f.get();

    std::cout << "  " << std::string(57, '-') << "\n";
    std::cout << "[+] Scan complete. " << aliveCount.load() << " host(s) responding on ARP.\n";
    return true;
}

} // namespace Netmap
