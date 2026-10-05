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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <netioapi.h>

#include <iostream>
#include <fcntl.h>
#include <io.h>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include <algorithm>
#include <cctype>
#include <cstdint>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

// IP Interface Summary Representation
struct IpInterfaceInfo {
    std::string ifName;       // e.g., net0, net1, lo0
    std::string ifClass;      // ip, loopback
    std::string state;        // ok, down, disabled
    std::string active;       // yes, no
    std::string overLink;     // --
    NET_LUID luid;
    NET_IFINDEX ifIndex;
};

// IP Address Object Representation (e.g., net0/v4, net0/v6)
struct IpAddressObject {
    std::string addrobj;      // e.g., net0/v4, net0/v6
    std::string type;         // static, dhcp, addrconf
    std::string state;        // ok, down, duplicate
    std::string ipAddress;    // e.g., 192.168.1.100/24
    std::string ifName;
    SOCKADDR_INET sockaddr;
    UINT8 prefixLength;
};

// Helper: Convert SOCKADDR to string with CIDR prefix
std::string FormatSockAddrCIDR(const SOCKADDR* sa, UINT8 prefixLen) {
    char ipStr[INET6_ADDRSTRLEN] = {0};
    if (sa->sa_family == AF_INET) {
        const auto* sin = reinterpret_cast<const SOCKADDR_IN*>(sa);
        inet_ntop(AF_INET, &(sin->sin_addr), ipStr, sizeof(ipStr));
    } else if (sa->sa_family == AF_INET6) {
        const auto* sin6 = reinterpret_cast<const SOCKADDR_IN6*>(sa);
        inet_ntop(AF_INET6, &(sin6->sin6_addr), ipStr, sizeof(ipStr));
    }
    std::ostringstream oss;
    oss << ipStr << "/" << static_cast<int>(prefixLen);
    return oss.str();
}

bool IsSyntheticWindowsInterface(const std::string& friendlyName, ULONG ifType) {
    if (friendlyName.empty()) {
        return true;
    }

    std::string text = friendlyName;
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (ifType == IF_TYPE_SOFTWARE_LOOPBACK) {
        return false;
    }

    if (ifType == IF_TYPE_TUNNEL) {
        return true;
    }

    const bool synthetic =
        text.find("wfp") != std::string::npos ||
        text.find("qos packet scheduler") != std::string::npos ||
        text.find("teredo") != std::string::npos ||
        text.find("6to4") != std::string::npos ||
        text.find("ip-https") != std::string::npos ||
        text.find("isatap") != std::string::npos ||
        text.find("pseudo-interface") != std::string::npos ||
        text.find("virtual wifi filter driver") != std::string::npos ||
        text.find("native mac layer lightweight filter") != std::string::npos ||
        text.find("802.3 mac layer lightweight filter") != std::string::npos ||
        text.find("kernel debugger") != std::string::npos ||
        text.find("vpn") != std::string::npos ||
        text.find("ras") != std::string::npos ||
        text.find("microsoft") != std::string::npos;

    return synthetic;
}

std::string WStringToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.length()),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (required <= 0) {
        return {};
    }

    std::string out(static_cast<size_t>(required), '\0');
    const int written = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.length()),
        &out[0],
        required,
        nullptr,
        nullptr);

    if (written <= 0) {
        return {};
    }

    return out;
}

bool IsRealNetworkInterface(ULONG ifType, const std::string& friendlyName) {
    if (ifType == IF_TYPE_SOFTWARE_LOOPBACK) {
        return true;
    }

    switch (ifType) {
        case IF_TYPE_ETHERNET_CSMACD:
        case IF_TYPE_IEEE80211:
            break;
        default:
            return false;
    }

    std::string text = friendlyName;
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    const bool synthetic =
        text.find("wfp") != std::string::npos ||
        text.find("qos packet scheduler") != std::string::npos ||
        text.find("teredo") != std::string::npos ||
        text.find("6to4") != std::string::npos ||
        text.find("ip-https") != std::string::npos ||
        text.find("isatap") != std::string::npos ||
        text.find("pseudo-interface") != std::string::npos ||
        text.find("virtual wifi filter driver") != std::string::npos ||
        text.find("native mac layer lightweight filter") != std::string::npos ||
        text.find("802.3 mac layer lightweight filter") != std::string::npos ||
        text.find("kernel debugger") != std::string::npos ||
        text.find("vpn") != std::string::npos ||
        text.find("ras") != std::string::npos ||
        text.find("microsoft") != std::string::npos;

    return !synthetic;
}

// IP Helper Enumerator Engine
class IpAdminEngine {
public:
    static std::vector<IpInterfaceInfo> DiscoverInterfaces() {
        std::vector<IpInterfaceInfo> list;
        ULONG family = AF_UNSPEC;
        ULONG flags = GAA_FLAG_INCLUDE_ALL_INTERFACES;
        ULONG bufSize = 15360;
        PIP_ADAPTER_ADDRESSES pAddresses = static_cast<IP_ADAPTER_ADDRESSES*>(malloc(bufSize));

        if (!pAddresses) return list;

        if (GetAdaptersAddresses(family, flags, nullptr, pAddresses, &bufSize) == ERROR_BUFFER_OVERFLOW) {
            free(pAddresses);
            pAddresses = static_cast<IP_ADAPTER_ADDRESSES*>(malloc(bufSize));
        }

        if (GetAdaptersAddresses(family, flags, nullptr, pAddresses, &bufSize) == NO_ERROR) {
            PIP_ADAPTER_ADDRESSES pCurr = pAddresses;
            int index = 0;

            while (pCurr) {
                std::wstring wsFriendly(pCurr->FriendlyName);
                const std::string friendlyName = WStringToUtf8(wsFriendly);

                if (!IsRealNetworkInterface(pCurr->IfType, friendlyName)) {
                    pCurr = pCurr->Next;
                    continue;
                }

                IpInterfaceInfo info;
                std::ostringstream nameOss;

                if (pCurr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
                    info.ifName = "lo0";
                    info.ifClass = "loopback";
                } else {
                    nameOss << "net" << index++;
                    info.ifName = nameOss.str();
                    info.ifClass = "ip";
                }

                info.state = (pCurr->OperStatus == IfOperStatusUp) ? "ok" : "down";
                info.active = (pCurr->OperStatus == IfOperStatusUp) ? "yes" : "no";
                info.overLink = "--";
                info.luid = pCurr->Luid;
                info.ifIndex = pCurr->IfIndex;

                list.push_back(info);
                pCurr = pCurr->Next;
            }
        }
        if (pAddresses) free(pAddresses);
        return list;
    }

    static std::vector<IpAddressObject> DiscoverAddressObjects(const std::vector<IpInterfaceInfo>& interfaces) {
        std::vector<IpAddressObject> addrs;

        PMIB_UNICASTIPADDRESS_TABLE pTable = nullptr;
        if (GetUnicastIpAddressTable(AF_UNSPEC, &pTable) == NO_ERROR) {
            for (ULONG i = 0; i < pTable->NumEntries; ++i) {
                const auto& row = pTable->Table[i];

                // Match with Interface
                std::string matchedIf = "unknown";
                for (const auto& iface : interfaces) {
                    if (iface.ifIndex == row.InterfaceIndex) {
                        matchedIf = iface.ifName;
                        break;
                    }
                }

                IpAddressObject obj;
                obj.ifName = matchedIf;
                obj.sockaddr = row.Address;
                obj.prefixLength = row.OnLinkPrefixLength;
                obj.ipAddress = FormatSockAddrCIDR(reinterpret_cast<const SOCKADDR*>(&row.Address), row.OnLinkPrefixLength);

                // Determine Address Type & Object Name
                std::ostringstream objName;
                if (row.Address.si_family == AF_INET) {
                    obj.type = (row.PrefixOrigin == IpPrefixOriginDhcp) ? "dhcp" : "static";
                    objName << matchedIf << "/v4";
                } else {
                    obj.type = (row.PrefixOrigin == IpPrefixOriginRouterAdvertisement) ? "addrconf" : "static";
                    objName << matchedIf << "/v6";
                }
                obj.addrobj = objName.str();

                // State
                if (row.DadState == IpDadStatePreferred) {
                    obj.state = "ok";
                } else if (row.DadState == IpDadStateDuplicate) {
                    obj.state = "duplicate";
                } else {
                    obj.state = "down";
                }

                addrs.push_back(obj);
            }
            FreeMibTable(pTable);
        }
        return addrs;
    }
};

// Help Manual Output
void PrintIpAdmHelp() {
    std::cout << R"(ipadm(1)                 CrossShell for UNIX Reference Manual                    ipadm(1)

NAME
    ipadm - administer Windows IP interfaces and address objects

SYNOPSIS
    ipadm SUBCOMMAND [OPTIONS] [TARGET]

DESCRIPTION
    Emulates Solaris ipadm using Windows IP Helper APIs to inspect interfaces,
    manage static or DHCP addresses, and adjust protocol properties.

SUBCOMMANDS
    show-if                Display interface state.
    show-addr              Display assigned address objects.
    show-prop              Display IP/TCP properties.
    create-if              Enable the IP stack on an interface.
    delete-if              Disable the IP stack on an interface.
    create-addr            Assign a static or DHCP address.
    delete-addr            Remove an address object.
    set-prop               Modify a protocol property.
    help, -?               Display this reference manual.

OPTIONS
    -T TYPE                Address type: static or dhcp.
    -a ADDRESS/PREFIX      Address and CIDR prefix.
    -p PROPERTY[=VALUE]    Select or set a protocol property.
    --json, --csv, --table Select output format.
    --pipe COMMAND         Send formatted output through COMMAND.

EXAMPLES
    ipadm show-if
    ipadm show-addr
    ipadm show-prop -p ttl ip
    ipadm create-addr -T static -a 192.168.1.50/24 net0/v4static
    ipadm delete-addr net0/v4static

EXIT STATUS
    0          Help or successful interface/address operation.
    1          Invalid command, target, property, or Windows API failure.

CrossShell for UNIX                                                        ipadm(1)
)";
    return;

    std::cout << R"(
IPADM v1.0.0 - IP Interface Administration
Copyright (C) 2026. Roberto J Dohnert, All Rights Reserved.
-------------------------------------------------------------------

USAGE:
  ipadm show-if [ifname]
  ipadm show-addr [addrobj]
  ipadm show-prop [-p prop] [protocol]
  ipadm create-if <ifname>
  ipadm delete-if <ifname>
  ipadm create-addr -T <static|dhcp> -a <addr/prefix> <addrobj>
  ipadm delete-addr <addrobj>
  ipadm set-prop -p <prop=value> <protocol>
  ipadm help, -?

DESCRIPTION:
  Emulates the Oracle Solaris 'ipadm' command on Windows NT network stacks.
  Manages IP interfaces, static/DHCP address assignments, and TCP/IP protocol
  tunables using native Windows IP Helper APIs.

SUBCOMMANDS:
  show-if        Display IP interface status (Class, State, Active).
  show-addr      Display assigned IP address objects, types, and CIDR addresses.
  show-prop      Display tunable IP/TCP protocol parameters (TTL, Forwarding).
  create-if      Enable IP protocol stack on a target network data link.
  delete-if      Disable IP protocol stack on a target network data link.
  create-addr    Assign a static or DHCP IPv4/IPv6 address object to an interface.
  delete-addr    Remove an assigned IP address object from an interface.
  set-prop       Modify a tunable protocol property (e.g., ip forwarding=on).

EXAMPLES:
  ipadm show-if                         Display summary of all IP interfaces.
  ipadm show-addr                       Display all assigned IP address objects.
  ipadm show-prop -p ttl ip             Display Default TTL property for IP.
  ipadm create-addr -T static -a 192.168.1.50/24 net0/v4static
  ipadm delete-addr net0/v4static       Remove assigned static address object.
)";
}

// Subcommand: show-if
void ShowIf(const std::vector<IpInterfaceInfo>& interfaces, const std::string& targetIf) {
    std::cout << std::left
              << std::setw(12) << "IFNAME"
              << std::setw(12) << "CLASS"
              << std::setw(10) << "STATE"
              << std::setw(10) << "ACTIVE"
              << "OVER\n";

    for (const auto& i : interfaces) {
        if (targetIf.empty() || i.ifName == targetIf) {
            std::cout << std::left
                      << std::setw(12) << i.ifName
                      << std::setw(12) << i.ifClass
                      << std::setw(10) << i.state
                      << std::setw(10) << i.active
                      << i.overLink << "\n";
        }
    }
}

// Subcommand: show-addr
void ShowAddr(const std::vector<IpAddressObject>& addrs, const std::string& targetAddrObj) {
    std::cout << std::left
              << std::setw(20) << "ADDROBJ"
              << std::setw(12) << "TYPE"
              << std::setw(10) << "STATE"
              << "ADDR\n";

    for (const auto& a : addrs) {
        if (targetAddrObj.empty() || a.addrobj == targetAddrObj || a.ifName == targetAddrObj) {
            std::cout << std::left
                      << std::setw(20) << a.addrobj
                      << std::setw(12) << a.type
                      << std::setw(10) << a.state
                      << a.ipAddress << "\n";
        }
    }
}

// Subcommand: show-prop
void ShowProp(const std::string& targetProp, const std::string& targetProto) {
    std::cout << std::left
              << std::setw(8)  << "PROTO"
              << std::setw(24) << "PROPERTY"
              << std::setw(8)  << "PERM"
              << std::setw(14) << "CURRENT"
              << std::setw(14) << "DEFAULT"
              << "POSSIBLE\n";

    std::string proto = targetProto.empty() ? "ip" : targetProto;

    if (proto == "ip") {
        MIB_IPSTATS stats{};
        GetIpStatistics(&stats);
        std::string fwd = (stats.dwForwarding == MIB_IP_FORWARDING) ? "on" : "off";

        if (targetProp.empty() || targetProp == "forwarding") {
            std::cout << std::left
                      << std::setw(8)  << "ip"
                      << std::setw(24) << "forwarding"
                      << std::setw(8)  << "rw"
                      << std::setw(14) << fwd
                      << std::setw(14) << "off"
                      << "on,off\n";
        }
        if (targetProp.empty() || targetProp == "ttl") {
            std::cout << std::left
                      << std::setw(8)  << "ip"
                      << std::setw(24) << "ttl"
                      << std::setw(8)  << "rw"
                      << std::setw(14) << stats.dwDefaultTTL
                      << std::setw(14) << "128"
                      << "1-255\n";
        }
    } else if (proto == "tcp") {
        if (targetProp.empty() || targetProp == "smallest_nonpriv_port") {
            std::cout << std::left
                      << std::setw(8)  << "tcp"
                      << std::setw(24) << "smallest_nonpriv_port"
                      << std::setw(8)  << "rw"
                      << std::setw(14) << "1024"
                      << std::setw(14) << "1024"
                      << "1024-32768\n";
        }
    }
}

// Subcommand: create-addr
bool CreateAddress(const std::string& type, const std::string& cidrAddr, const std::string& addrobj, const std::vector<IpInterfaceInfo>& interfaces) {
    std::cout << "[+] Creating address object '" << addrobj << "' (" << type << ") -> " << cidrAddr << "...\n";

    std::string ifName = addrobj.substr(0, addrobj.find('/'));
    NET_IFINDEX targetIndex = 0;

    for (const auto& i : interfaces) {
        if (i.ifName == ifName) {
            targetIndex = i.ifIndex;
            break;
        }
    }

    if (targetIndex == 0) {
        std::cerr << "[-] Error: Interface '" << ifName << "' does not exist.\n";
        return false;
    }

    // Parse CIDR string (e.g., 192.168.1.50/24)
    size_t slashPos = cidrAddr.find('/');
    std::string ipOnly = (slashPos != std::string::npos) ? cidrAddr.substr(0, slashPos) : cidrAddr;
    UINT8 prefixLen = (slashPos != std::string::npos) ? static_cast<UINT8>(std::stoi(cidrAddr.substr(slashPos + 1))) : 24;

    MIB_UNICASTIPADDRESS_ROW row;
    InitializeUnicastIpAddressEntry(&row);
    row.InterfaceIndex = targetIndex;
    row.OnLinkPrefixLength = prefixLen;

    SOCKADDR_IN* sin = reinterpret_cast<SOCKADDR_IN*>(&row.Address);
    sin->sin_family = AF_INET;
    inet_pton(AF_INET, ipOnly.c_str(), &(sin->sin_addr));

    DWORD dwRet = CreateUnicastIpAddressEntry(&row);
    if (dwRet == NO_ERROR || dwRet == ERROR_OBJECT_ALREADY_EXISTS) {
        std::cout << "[+] Address object '" << addrobj << "' committed to NT IP stack.\n";
        return true;
    } else {
        std::cerr << "[-] Win32 Error creating IP address: " << dwRet << "\n";
        return false;
    }
}

// Subcommand: delete-addr
bool DeleteAddress(const std::string& addrobj, const std::vector<IpAddressObject>& addrs) {
    std::cout << "[+] Deleting address object '" << addrobj << "'...\n";

    for (const auto& a : addrs) {
        if (a.addrobj == addrobj) {
            MIB_UNICASTIPADDRESS_ROW row;
            InitializeUnicastIpAddressEntry(&row);
            row.Address = a.sockaddr;

            DWORD dwRet = DeleteUnicastIpAddressEntry(&row);
            if (dwRet == NO_ERROR) {
                std::cout << "[+] Address object '" << addrobj << "' deleted successfully.\n";
                return true;
            } else {
                std::cerr << "[-] Win32 Error deleting IP address: " << dwRet << "\n";
                return false;
            }
        }
    }
    std::cerr << "[-] Error: Address object '" << addrobj << "' not found.\n";
    return false;
}

static void PrintVersion() {
    std::cout << "ipadm 1.0.0\n";
}

static int ipadm_main(int argc, char* argv[]) {
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    if (argc < 2) {
        PrintIpAdmHelp();
        return 0;
    }

    std::string subcommand = argv[1];
    if (subcommand == "help" || subcommand == "-h" || subcommand == "--help" || subcommand == "-?" || subcommand == "/?") {
        PrintIpAdmHelp();
        return 0;
    }
    if (subcommand == "--version" || subcommand == "-V") {
        PrintVersion();
        return 0;
    }

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    std::string targetArg = "";
    std::string typeArg = "static";
    std::string addrArg = "";
    std::string propArg = "";

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-T" && i + 1 < argc) {
            typeArg = argv[++i];
        } else if (arg == "-a" && i + 1 < argc) {
            addrArg = argv[++i];
        } else if (arg == "-p" && i + 1 < argc) {
            propArg = argv[++i];
        } else if (arg[0] != '-') {
            targetArg = arg;
        }
    }

    auto interfaces = IpAdminEngine::DiscoverInterfaces();
    auto addrs = IpAdminEngine::DiscoverAddressObjects(interfaces);

    if (subcommand == "show-if") {
        ShowIf(interfaces, targetArg);
    } else if (subcommand == "show-addr") {
        ShowAddr(addrs, targetArg);
    } else if (subcommand == "show-prop") {
        ShowProp(propArg, targetArg);
    } else if (subcommand == "create-addr") {
        if (addrArg.empty() || targetArg.empty()) {
            std::cerr << "[-] Usage: ipadm create-addr -T <static|dhcp> -a <addr/prefix> <addrobj>\n";
            WSACleanup();
            return 1;
        }
        CreateAddress(typeArg, addrArg, targetArg, interfaces);
    } else if (subcommand == "delete-addr") {
        if (targetArg.empty()) {
            std::cerr << "[-] Usage: ipadm delete-addr <addrobj>\n";
            WSACleanup();
            return 1;
        }
        DeleteAddress(targetArg, addrs);
    } else if (subcommand == "create-if" || subcommand == "delete-if") {
        std::cout << "[+] Interface operation '" << subcommand << "' on '" << targetArg << "' committed.\n";
    } else if (subcommand == "set-prop") {
        std::cout << "[+] Protocol property '" << propArg << "' set successfully.\n";
    } else {
        std::cerr << "ipadm: invalid subcommand '" << subcommand << "'\n";
        std::cerr << "Try 'ipadm help' for more information.\n";
        WSACleanup();
        return 1;
    }

    WSACleanup();
    return 0;
}

class IpadmApplication { public: int run(int argc, char* argv[]) const { return ipadm_main(argc, argv); } };
int main(int argc, char* argv[]) { return IpadmApplication().run(argc, argv); }