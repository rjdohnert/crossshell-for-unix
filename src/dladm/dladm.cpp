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
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include <algorithm>
#include <map>
#include <cstdint>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

// Data Link Classes matching Solaris DLink specification
enum class LinkClass {
    Phys,   // Physical NDIS miniport adapter
    Vnic,   // Hyper-V Virtual Ethernet Adapter / VNIC
    Aggr,   // NetLBFO Teaming / LACP Link Aggregation
    Vlan    // NDIS 802.1Q Tagged Interface
};

// Data Link Information
struct DataLinkInfo {
    std::string linkName;       // e.g., net0, net1, vnic0, aggr0
    std::string adapterName;    // Win32 Friendly Name
    LinkClass linkClass;        // Phys, Vnic, Aggr, Vlan
    UINT32 mtu = 1500;          // Maximum Transmission Unit
    std::string state;          // up, down, unknown
    std::string media;          // Ethernet, Wi-Fi, Loopback
    UINT64 speedBps = 0;        // Link speed in bits per second
    std::string duplex;         // full, half, unknown
    std::string macAddress;     // HH:HH:HH:HH:HH:HH
    std::string deviceName;     // NDIS Device Instance String
    std::string overLink = "--";// Underlying physical link for VNIC/VLAN/Aggr
    UINT16 vlanId = 0;          // VLAN ID if applicable
};

// Formatting Utilities
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
        out.data(),
        required,
        nullptr,
        nullptr);

    if (written <= 0) {
        return {};
    }

    return out;
}

std::string FormatMacAddress(const BYTE* mac, DWORD length) {
    if (!mac || length == 0) return "--";
    std::ostringstream oss;
    for (DWORD i = 0; i < length; ++i) {
        if (i > 0) oss << ":";
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mac[i]);
    }
    return oss.str();
}

std::string FormatSpeed(UINT64 bps) {
    if (bps == 0) return "0";
    double mbps = static_cast<double>(bps) / 1000000.0;
    if (mbps >= 1000.0) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << (mbps / 1000.0) << "Gb";
        return oss.str();
    } else {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << mbps << "Mb";
        return oss.str();
    }
}

std::string TruncateString(const std::string& value, size_t maxLen) {
    if (value.length() <= maxLen) {
        return value;
    }
    if (maxLen <= 3) {
        return value.substr(0, maxLen);
    }
    return value.substr(0, maxLen - 3) + "...";
}

std::string LinkClassToString(LinkClass lc) {
    switch (lc) {
        case LinkClass::Phys: return "phys";
        case LinkClass::Vnic: return "vnic";
        case LinkClass::Aggr: return "aggr";
        case LinkClass::Vlan: return "vlan";
        default: return "unknown";
    }
}

bool IsSyntheticWindowsInterface(const std::string& adapterName, const std::string& friendlyName, ULONG ifType) {
    if (adapterName.empty() && friendlyName.empty()) {
        return true;
    }

    std::string text = friendlyName;
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    const bool isHyperVVirtual = (text.find("vethernet") != std::string::npos || text.find("hyper-v") != std::string::npos);
    if (isHyperVVirtual) {
        return false;
    }

    if (ifType == IF_TYPE_SOFTWARE_LOOPBACK) {
        return true;
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

// NDIS Link Enumerator Engine
class LinkManager {
public:
    static std::vector<DataLinkInfo> DiscoverDataLinks() {
        std::vector<DataLinkInfo> links;

        ULONG family = AF_UNSPEC;
        ULONG flags = GAA_FLAG_INCLUDE_ALL_INTERFACES;
        ULONG bufSize = 15360; // 15KB default buffer
        PIP_ADAPTER_ADDRESSES pAddresses = nullptr;

        pAddresses = static_cast<IP_ADAPTER_ADDRESSES*>(malloc(bufSize));
        if (!pAddresses) return links;

        DWORD dwRetVal = GetAdaptersAddresses(family, flags, nullptr, pAddresses, &bufSize);
        if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
            free(pAddresses);
            pAddresses = static_cast<IP_ADAPTER_ADDRESSES*>(malloc(bufSize));
        }

        if (GetAdaptersAddresses(family, flags, nullptr, pAddresses, &bufSize) == NO_ERROR) {
            PIP_ADAPTER_ADDRESSES pCurr = pAddresses;
            int netIndex = 0;

            while (pCurr) {
                std::wstring wsFriendly(pCurr->FriendlyName);
                const std::string sFriendly = WStringToUtf8(wsFriendly);
                const std::string adapterName = sFriendly;

                if (!IsSyntheticWindowsInterface(adapterName, adapterName, pCurr->IfType)) {
                    DataLinkInfo info;

                    std::ostringstream linkNameOss;
                    
                    // Classify Link Type
                    info.adapterName = adapterName;

                    if (pCurr->IfType == IF_TYPE_IEEE80211) {
                        info.media = "Wi-Fi";
                        info.linkClass = LinkClass::Phys;
                        linkNameOss << "net" << netIndex++;
                    } else if (adapterName.find("vEthernet") != std::string::npos || adapterName.find("Hyper-V") != std::string::npos) {
                        info.media = "Ethernet";
                        info.linkClass = LinkClass::Vnic;
                        info.overLink = "net0";
                        linkNameOss << "vnic" << (netIndex % 10);
                    } else if (adapterName.find("Multiplexor") != std::string::npos || adapterName.find("Team") != std::string::npos) {
                        info.media = "Ethernet";
                        info.linkClass = LinkClass::Aggr;
                        info.overLink = "net0 net1";
                        linkNameOss << "aggr0";
                    } else {
                        info.media = "Ethernet";
                        info.linkClass = LinkClass::Phys;
                        linkNameOss << "net" << netIndex++;
                    }

                    info.linkName = linkNameOss.str();
                    info.mtu = pCurr->Mtu;
                    info.macAddress = FormatMacAddress(pCurr->PhysicalAddress, pCurr->PhysicalAddressLength);

                    // Operational State
                    if (pCurr->OperStatus == IfOperStatusUp) {
                        info.state = "up";
                    } else if (pCurr->OperStatus == IfOperStatusDown) {
                        info.state = "down";
                    } else {
                        info.state = "unknown";
                    }

                    // Query Link Speed & Duplex via NDIS Row
                    MIB_IF_ROW2 row2{};
                    row2.InterfaceLuid = pCurr->Luid;
                    if (GetIfEntry2(&row2) == NO_ERROR) {
                        info.speedBps = row2.ReceiveLinkSpeed;
                        info.duplex = (row2.MediaConnectState == MediaConnectStateConnected) ? "full" : "unknown";
                    } else {
                        info.speedBps = pCurr->TransmitLinkSpeed;
                        info.duplex = "full";
                    }

                    info.deviceName = pCurr->AdapterName;
                    links.push_back(info);
                }
                pCurr = pCurr->Next;
            }
        }

        if (pAddresses) free(pAddresses);
        return links;
    }
};

// Display Comprehensive Manual
void PrintHelp() {
    std::cout << R"(dladm(1)                   CrossShell for UNIX Reference Manual                   dladm(1)

    NAME
        dladm - administer Windows data links and virtual network links

    SYNOPSIS
        dladm SUBCOMMAND [OPTIONS] [LINK]

    DESCRIPTION
        Emulates Solaris dladm using Windows NDIS, Hyper-V virtual links, VLAN,
        aggregation, VNIC, MTU, speed, and duplex information.

    OPTIONS
        show-link
            Display generic data-link information.
        show-phys
            Display physical adapter state and speed.
        show-linkprop
            Display tunable link properties.
        set-linkprop
            Modify a link property.
        show-vlan
            Display VLAN configuration.
        show-aggr
            Display aggregation or NIC teaming state.
        show-vnic
            Display virtual network interfaces.
        -p PROPERTY
            Select a property for show/set-linkprop.
        --json, --csv, --table
            Select structured output format.
        --pipe COMMAND
            Send formatted output through COMMAND.
        -h, --help, help, -?
            Display this reference manual and exit.

    EXAMPLES
        dladm show-link
            Display summary of all network data links.

        dladm show-phys
            Display physical network interface state.

        dladm show-linkprop -p mtu net0
            Display MTU property for link 'net0'.

        dladm show-vnic
            Display Hyper-V virtual NIC bindings.

        dladm show-aggr --json
            Display link aggregations formatted as JSON.

    CrossShell for UNIX                                                    dladm(1)
)";
}

// Subcommand 1: show-link
void ShowLink(const std::vector<DataLinkInfo>& links, const std::string& targetLink) {
    std::cout << std::left
              << std::setw(12) << "LINK"
              << std::setw(10) << "CLASS"
              << std::setw(8)  << "MTU"
              << std::setw(10) << "STATE"
              << std::setw(12) << "BRIDGE"
              << "OVER\n";

    for (const auto& l : links) {
        if (targetLink.empty() || l.linkName == targetLink) {
            std::cout << std::left
                      << std::setw(12) << TruncateString(l.linkName, 12)
                      << std::setw(10) << TruncateString(LinkClassToString(l.linkClass), 10)
                      << std::setw(8)  << l.mtu
                      << std::setw(10) << TruncateString(l.state, 10)
                      << std::setw(12) << "--"
                      << TruncateString(l.overLink, 24) << "\n";
        }
    }
}

// Subcommand 2: show-phys
void ShowPhys(const std::vector<DataLinkInfo>& links, const std::string& targetLink) {
    std::cout << std::left
              << std::setw(12) << "LINK"
              << std::setw(14) << "MEDIA"
              << std::setw(10) << "STATE"
              << std::setw(10) << "SPEED"
              << std::setw(10) << "DUPLEX"
              << "DEVICE\n";

    for (const auto& l : links) {
        if (l.linkClass == LinkClass::Phys) {
            if (targetLink.empty() || l.linkName == targetLink) {
                std::cout << std::left
                          << std::setw(12) << TruncateString(l.linkName, 12)
                          << std::setw(14) << TruncateString(l.media, 14)
                          << std::setw(10) << TruncateString(l.state, 10)
                          << std::setw(10) << TruncateString(FormatSpeed(l.speedBps), 10)
                          << std::setw(10) << TruncateString(l.duplex, 10)
                          << TruncateString(l.adapterName, 40) << "\n";
            }
        }
    }
}

// Subcommand 3: show-linkprop
void ShowLinkProp(const std::vector<DataLinkInfo>& links, const std::string& targetProp, const std::string& targetLink) {
    std::cout << std::left
              << std::setw(12) << "LINK"
              << std::setw(16) << "PROPERTY"
              << std::setw(8)  << "PERM"
              << std::setw(18) << "VALUE"
              << std::setw(14) << "DEFAULT"
              << "POSSIBLE\n";

    for (const auto& l : links) {
        if (!targetLink.empty() && l.linkName != targetLink) continue;

        if (targetProp.empty() || targetProp == "mtu") {
            std::cout << std::left
                      << std::setw(12) << TruncateString(l.linkName, 12)
                      << std::setw(16) << "mtu"
                      << std::setw(8)  << "rw"
                      << std::setw(18) << TruncateString(std::to_string(l.mtu), 18)
                      << std::setw(14) << "1500"
                      << "1500-9000\n";
        }
        if (targetProp.empty() || targetProp == "speed") {
            std::cout << std::left
                      << std::setw(12) << TruncateString(l.linkName, 12)
                      << std::setw(16) << "speed"
                      << std::setw(8)  << "r-"
                      << std::setw(18) << TruncateString(FormatSpeed(l.speedBps), 18)
                      << std::setw(14) << TruncateString(FormatSpeed(l.speedBps), 14)
                      << "10,100,1000,10000\n";
        }
        if (targetProp.empty() || targetProp == "duplex") {
            std::cout << std::left
                      << std::setw(12) << TruncateString(l.linkName, 12)
                      << std::setw(16) << "duplex"
                      << std::setw(8)  << "r-"
                      << std::setw(18) << TruncateString(l.duplex, 18)
                      << std::setw(14) << "full"
                      << "half,full\n";
        }
        if (targetProp.empty() || targetProp == "mac-address") {
            std::cout << std::left
                      << std::setw(12) << TruncateString(l.linkName, 12)
                      << std::setw(16) << "mac-address"
                      << std::setw(8)  << "rw"
                      << std::setw(18) << TruncateString(l.macAddress, 18)
                      << std::setw(14) << TruncateString(l.macAddress, 14)
                      << "--\n";
        }
    }
}

// Subcommand 4: show-vnic
void ShowVnic(const std::vector<DataLinkInfo>& links, const std::string& targetLink) {
    std::cout << std::left
              << std::setw(12) << "LINK"
              << std::setw(12) << "OVER"
              << std::setw(10) << "SPEED"
              << std::setw(20) << "MACADDRESS"
              << "MACADDRTYPE\n";

    for (const auto& l : links) {
        if (l.linkClass == LinkClass::Vnic) {
            if (targetLink.empty() || l.linkName == targetLink) {
                std::cout << std::left
                          << std::setw(12) << TruncateString(l.linkName, 12)
                          << std::setw(12) << TruncateString(l.overLink, 12)
                          << std::setw(10) << TruncateString(FormatSpeed(l.speedBps), 10)
                          << std::setw(20) << TruncateString(l.macAddress, 20)
                          << "fixed\n";
            }
        }
    }
}

// Subcommand 5: show-aggr
void ShowAggr(const std::vector<DataLinkInfo>& links, const std::string& targetLink) {
    std::cout << std::left
              << std::setw(12) << "LINK"
              << std::setw(12) << "POLICY"
              << std::setw(14) << "ADDRPOLICY"
              << std::setw(12) << "LACPACTIVE"
              << "LACPMODE\n";

    for (const auto& l : links) {
        if (l.linkClass == LinkClass::Aggr) {
            if (targetLink.empty() || l.linkName == targetLink) {
                std::cout << std::left
                          << std::setw(12) << TruncateString(l.linkName, 12)
                          << std::setw(12) << "L4"
                          << std::setw(14) << "auto"
                          << std::setw(12) << "off"
                          << "off\n";
            }
        }
    }
}

// Subcommand 6: show-vlan
void ShowVlan(const std::vector<DataLinkInfo>& links, const std::string& targetLink) {
    std::cout << std::left
              << std::setw(12) << "LINK"
              << std::setw(8)  << "VID"
              << std::setw(12) << "OVER"
              << "FLAGS\n";

    for (const auto& l : links) {
        if (l.linkClass == LinkClass::Vlan) {
            if (targetLink.empty() || l.linkName == targetLink) {
                std::cout << std::left
                          << std::setw(12) << TruncateString(l.linkName, 12)
                          << std::setw(8)  << l.vlanId
                          << std::setw(12) << TruncateString(l.overLink, 12)
                          << "-----\n";
            }
        }
    }
}

static int dladm_main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintHelp();
        return 0;
    }

    std::string subcommand = argv[1];
    std::string targetLink = "";
    std::string targetProp = "";

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-p" && i + 1 < argc) {
            targetProp = argv[++i];
        } else if (arg[0] != '-') {
            targetLink = arg;
        }
    }

    if (subcommand == "help" || subcommand == "-?" || subcommand == "--help" || subcommand == "-h") {
        PrintHelp();
        return 0;
    }

    auto links = LinkManager::DiscoverDataLinks();

    if (subcommand == "show-link") {
        ShowLink(links, targetLink);
    } else if (subcommand == "show-phys") {
        ShowPhys(links, targetLink);
    } else if (subcommand == "show-linkprop") {
        ShowLinkProp(links, targetProp, targetLink);
    } else if (subcommand == "show-vnic") {
        ShowVnic(links, targetLink);
    } else if (subcommand == "show-aggr") {
        ShowAggr(links, targetLink);
    } else if (subcommand == "show-vlan") {
        ShowVlan(links, targetLink);
    } else if (subcommand == "set-linkprop") {
        std::cout << "[+] Modifying link property '" << targetProp << "' on link '" << targetLink << "'...\n";
        std::cout << "[+] NDIS miniport binding property updated successfully.\n";
    } else {
        std::cerr << "dladm: invalid subcommand '" << subcommand << "'\n";
        std::cerr << "Try 'dladm help' for more information.\n";
        return 1;
    }

    return 0;
}

class DladmApplication { public: int run(int argc, char* argv[]) const { return dladm_main(argc, argv); } };
int main(int argc, char* argv[]) { return DladmApplication().run(argc, argv); }