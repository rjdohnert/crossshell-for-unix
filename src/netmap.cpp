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
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")

#include <iostream>
#include <iomanip>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <optional>
#include <map>
#include <set>
#include <chrono>
#include <future>
#include <mutex>
#include <algorithm>
#include <regex>

namespace Netmap {

// ============================================================================
// Data Structures & Enums
// ============================================================================

enum class Mode {
    Display,        // -a, -g
    Add,            // -s
    Delete,         // -d
    Flush,          // -f, --flush
    Scan,           // -scan
    ListInterfaces, // -if, --interfaces
    Help,           // -h, --help, /?
    Version         // -v, --version
};

struct InterfaceInfo {
    NET_IFINDEX index = 0;
    NET_LUID luid{};
    std::string name;
    std::string friendlyName;
    std::string description;
    std::vector<std::string> ipv4Addresses;
    std::string macAddress;
    ULONG mtu = 0;
    IF_OPER_STATUS status = IfOperStatusDown;
};

struct ArpEntry {
    std::string ipAddress;
    std::string macAddress;
    std::string stateStr;
    std::string typeStr;
    NET_IFINDEX ifIndex = 0;
    std::string ifIp;
    bool isStatic = false;
};

// ============================================================================
// Utility Functions
// ============================================================================

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

// ============================================================================
// Interface Manager
// ============================================================================

class InterfaceManager {
public:
    static std::vector<InterfaceInfo> GetAllInterfaces() {
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

    static std::optional<InterfaceInfo> FindByIpOrIndex(const std::string& identifier) {
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

    static std::optional<InterfaceInfo> ResolveBestInterfaceForIp(const std::string& targetIp) {
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
};

// ============================================================================
// Core ARP Engine
// ============================================================================

class ArpEngine {
public:
    static bool DisplayTable(const std::optional<std::string>& ipFilter, 
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

        // Group entries by Interface Index
        std::map<NET_IFINDEX, std::vector<MIB_IPNET_ROW2>> groupedRows;
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            const auto& row = table->Table[i];
            
            // Check IP Filter
            char ipStr[INET_ADDRSTRLEN] = {0};
            InetNtopA(AF_INET, &(row.Address.Ipv4.sin_addr), ipStr, sizeof(ipStr));

            if (ipFilter && *ipFilter != ipStr) continue;

            // Check Interface Filter
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

    static bool AddEntry(const std::string& ipAddress, const std::string& macAddress, 
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

        // Try creating entry; if exists, update it
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

    static bool DeleteEntry(const std::string& ipAddress, const std::optional<std::string>& ifIdentifier) {
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

    static bool FlushEntries(const std::optional<std::string>& ifIdentifier) {
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

    static bool ScanSubnet(const std::string& cidrOrRange, int timeoutMs = 800, int threadCount = 32) {
        std::vector<uint32_t> targetIps;

        // Parse CIDR (e.g. 192.168.1.0/24) or Range (192.168.1.1-192.168.1.254) or base prefix
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
            // Single host probe
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
};

// ============================================================================
// Help and Information System
// ============================================================================

class HelpSystem {
public:
    static void ShowVersion() {
        std::cout << "netmap 1.2.0\n"
                  << "\n"
                  << "Copyright (C) 2026. All rights reserved.\n";
    }

    static void ShowHelp(const std::string& section = "") {
        if (section == "add" || section == "-s") {
            ShowAddHelp();
            return;
        } else if (section == "delete" || section == "-d") {
            ShowDeleteHelp();
            return;
        } else if (section == "scan" || section == "-scan") {
            ShowScanHelp();
            return;
        }

        std::cout << R"(netmap(1)               CrossShell for UNIX Reference Manual                  netmap(1)

    NAME
        netmap - ARP cache management and network mapping utility

    SYNOPSIS
        netmap [OPTIONS] [ARGUMENTS...]

    DESCRIPTION
        netmap provides high-performance ARP table inspection, address
        resolution management, and active local network discovery sweeps.
        It interfaces directly with the Windows IP Helper and Winsock APIs.

    OPTIONS
        -a, -g [INET_ADDR]
            Display current ARP table entries, optionally filtered by IP address.

        -s INET_ADDR ETH_ADDR
            Add a static ARP binding mapping INET_ADDR to physical ETH_ADDR.

        -d INET_ADDR
            Delete the ARP entry associated with INET_ADDR (use * for all).

        -f, --flush
            Flush the entire ARP cache across all network interfaces.

        --scan CIDR_OR_RANGE
            Perform an active multi-threaded ARP discovery sweep across a subnet.

        -if, --interfaces
            List all active network interfaces with IDs and addresses.

        -N IF_ADDR_OR_INDEX
            Specify target interface by its IP address or interface index.

        -v, --verbose
            Display unresolvable, stale, and probe-state entries.

        -t, --timeout MS
            Set timeout for active discovery scans in milliseconds (default: 800).

        --threads NUM
            Number of concurrent threads for active subnet scanning (default: 32).

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        netmap -a
            Display complete ARP table across all interfaces.

        netmap -a 192.168.1.1
            Look up ARP entry specifically for gateway 192.168.1.1.

        netmap -s 192.168.1.250 00-11-22-33-44-55
            Add static IP-to-MAC hardware binding.

        netmap --scan 192.168.1.0/24
            Discover all live devices on the local subnet via ARP sweep.

        netmap --flush
            Flush the ARP cache for all network adapters.

    CrossShell for UNIX                                                    netmap(1)
)";
    }

    static void ShowAddHelp() {
        std::cout << "NETMAP - Static Entry Creation Help (-s)\n\n"
                  << "SYNOPSIS:\n"
                  << "    netmap -s <inet_addr> <eth_addr> [-N <if_addr|if_index>]\n\n"
                  << "DESCRIPTION:\n"
                  << "    Creates a permanent, static binding in the ARP cache between an IPv4 address\n"
                  << "    and a physical MAC address. If no interface is specified with -N, netmap\n"
                  << "    automatically calculates the best interface route via Windows Routing Engine.\n\n"
                  << "MAC FORMATS ACCEPTED:\n"
                  << "    00-1A-2B-3C-4D-5E   (Standard Windows dash-separated)\n"
                  << "    00:1A:2B:3C:4D:5E   (UNIX colon-separated)\n"
                  << "    001A2B3C4D5E         (Continuous hexadecimal string)\n\n"
                  << "EXAMPLE:\n"
                  << "    netmap -s 192.168.1.100 a4-83-e7-21-99-0a\n"
                  << "    netmap -s 10.0.0.254 00:50:56:c0:00:08 -N 10.0.0.15\n";
    }

    static void ShowDeleteHelp() {
        std::cout << "NETMAP - Entry Deletion Help (-d, -f)\n\n"
                  << "SYNOPSIS:\n"
                  << "    netmap -d <inet_addr|*> [-N <if_addr|if_index>]\n"
                  << "    netmap -f | --flush [-N <if_addr|if_index>]\n\n"
                  << "DESCRIPTION:\n"
                  << "    Removes dynamic or static mapping entries from the system neighbor cache.\n"
                  << "    Specifying '*' as the address or running -f will purge the table completely.\n\n"
                  << "EXAMPLES:\n"
                  << "    netmap -d 192.168.1.100\n"
                  << "    netmap -d * -N 192.168.1.50\n"
                  << "    netmap --flush\n";
    }

    static void ShowScanHelp() {
        std::cout << "NETMAP - Active ARP Scanner Help (-scan)\n\n"
                  << "SYNOPSIS:\n"
                  << "    netmap -scan <cidr_block | ip_range | single_ip> [-t <timeout_ms>] [-threads <count>]\n\n"
                  << "DESCRIPTION:\n"
                  << "    Directly issues raw ARP request packets using the SendARP kernel pipeline to\n"
                  << "    detect active physical hosts on the local Ethernet / Wi-Fi layer. Bypasses\n"
                  << "    ICMP firewalls that block standard ping packets.\n\n"
                  << "EXAMPLES:\n"
                  << "    netmap -scan 192.168.1.0/24\n"
                  << "    netmap -scan 192.168.1.1-192.168.1.50 -threads 16\n"
                  << "    netmap -scan 10.0.0.1 -t 1500\n";
    }

    static void ShowInterfaces() {
        auto ifaces = InterfaceManager::GetAllInterfaces();
        std::cout << "\nAvailable Network Interfaces (" << ifaces.size() << " detected):\n";
        std::cout << std::string(75, '=') << "\n";

        for (const auto& iface : ifaces) {
            std::cout << "Index: " << iface.index << " (0x" << std::hex << iface.index << std::dec << ")\n"
                      << "  Friendly Name : " << iface.friendlyName << "\n"
                      << "  Description   : " << iface.description << "\n"
                      << "  MAC Address   : " << iface.macAddress << "\n"
                      << "  Status        : " << (iface.status == IfOperStatusUp ? "UP / ACTIVE" : "DOWN") << "\n"
                      << "  MTU           : " << iface.mtu << "\n"
                      << "  IPv4 Addrs    : ";
            if (iface.ipv4Addresses.empty()) {
                std::cout << "[None Assigned]\n";
            } else {
                for (size_t i = 0; i < iface.ipv4Addresses.size(); ++i) {
                    std::cout << iface.ipv4Addresses[i] << (i + 1 < iface.ipv4Addresses.size() ? ", " : "");
                }
                std::cout << "\n";
            }
            std::cout << std::string(75, '-') << "\n";
        }
    }
};

} // namespace Netmap

// ============================================================================
// Main Command Line Parser
// ============================================================================

int main(int argc, char* argv[]) {
    // Initialize Windows Sockets
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[-] Failed to initialize Windows Sockets layer.\n";
        return 1;
    }

    if (argc < 2) {
        // Default behavior matching Windows ARP utility: show table
        Netmap::ArpEngine::DisplayTable(std::nullopt, std::nullopt, false);
        WSACleanup();
        return 0;
    }

    Netmap::Mode mode = Netmap::Mode::Display;
    std::optional<std::string> targetIp;
    std::optional<std::string> targetMac;
    std::optional<std::string> ifFilter;
    std::optional<std::string> scanTarget;
    bool verbose = false;
    int timeoutMs = 800;
    int threadCount = 32;
    std::string helpTopic;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-a" || arg == "-g") {
            mode = Netmap::Mode::Display;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                targetIp = argv[++i];
            }
        } else if (arg == "-s") {
            mode = Netmap::Mode::Add;
            if (i + 2 < argc) {
                targetIp = argv[++i];
                targetMac = argv[++i];
            } else {
                std::cerr << "[-] Error: -s requires both <inet_addr> and <eth_addr>.\n"
                          << "    Use 'netmap -h add' for syntax assistance.\n";
                WSACleanup();
                return 1;
            }
        } else if (arg == "-d") {
            mode = Netmap::Mode::Delete;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                targetIp = argv[++i];
            } else {
                targetIp = "*"; // Default to flush all if no IP supplied
            }
        } else if (arg == "-f" || arg == "--flush") {
            mode = Netmap::Mode::Flush;
        } else if (arg == "-scan" || arg == "--scan") {
            mode = Netmap::Mode::Scan;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                scanTarget = argv[++i];
            } else {
                std::cerr << "[-] Error: -scan requires a subnet CIDR (e.g. 192.168.1.0/24) or range.\n";
                WSACleanup();
                return 1;
            }
        } else if (arg == "-if" || arg == "--interfaces") {
            mode = Netmap::Mode::ListInterfaces;
        } else if (arg == "-N") {
            if (i + 1 < argc) {
                ifFilter = argv[++i];
            } else {
                std::cerr << "[-] Error: -N requires an interface IP address or index.\n";
                WSACleanup();
                return 1;
            }
        } else if (arg == "-v" || arg == "-verbose") {
            verbose = true;
        } else if (arg == "--version") {
            mode = Netmap::Mode::Version;
        } else if (arg == "-h" || arg == "--help" || arg == "/?") {
            mode = Netmap::Mode::Help;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                helpTopic = argv[++i];
            }
        } else if (arg == "-t" || arg == "--timeout") {
            if (i + 1 < argc) {
                timeoutMs = std::stoi(argv[++i]);
            }
        } else if (arg == "-threads") {
            if (i + 1 < argc) {
                threadCount = std::stoi(argv[++i]);
            }
        } else if (targetIp == std::nullopt && Netmap::IsValidIpv4(arg)) {
            targetIp = arg;
        } else {
            std::cerr << "[-] Unrecognized option: " << arg << "\n"
                      << "    Use 'netmap --help' to display available syntax and modes.\n";
            WSACleanup();
            return 1;
        }
    }

    bool success = true;
    switch (mode) {
        case Netmap::Mode::Display:
            success = Netmap::ArpEngine::DisplayTable(targetIp, ifFilter, verbose);
            break;
        case Netmap::Mode::Add:
            if (targetIp && targetMac) {
                success = Netmap::ArpEngine::AddEntry(*targetIp, *targetMac, ifFilter);
            }
            break;
        case Netmap::Mode::Delete:
            if (targetIp) {
                success = Netmap::ArpEngine::DeleteEntry(*targetIp, ifFilter);
            }
            break;
        case Netmap::Mode::Flush:
            success = Netmap::ArpEngine::FlushEntries(ifFilter);
            break;
        case Netmap::Mode::Scan:
            if (scanTarget) {
                success = Netmap::ArpEngine::ScanSubnet(*scanTarget, timeoutMs, threadCount);
            }
            break;
        case Netmap::Mode::ListInterfaces:
            Netmap::HelpSystem::ShowInterfaces();
            break;
        case Netmap::Mode::Help:
            Netmap::HelpSystem::ShowHelp(helpTopic);
            break;
        case Netmap::Mode::Version:
            Netmap::HelpSystem::ShowVersion();
            break;
    }

    WSACleanup();
    return success ? 0 : 1;
}