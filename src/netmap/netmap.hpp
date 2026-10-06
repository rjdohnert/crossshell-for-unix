/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * netmap - ARP cache management and network mapping utility
 */

#ifndef NETMAP_HPP
#define NETMAP_HPP

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>

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

// Utility function prototypes
std::string FormatWin32Error(DWORD errorCode);
std::string WideToUtf8(const std::wstring& wstr);
bool IsUserAdmin();
std::string FormatMac(const uint8_t* mac, ULONG len);
bool ParseMac(const std::string& str, uint8_t mac[6]);
bool IsValidIpv4(const std::string& ip);
std::string StateToString(NL_NEIGHBOR_STATE state);

} // namespace Netmap

#endif // NETMAP_HPP
