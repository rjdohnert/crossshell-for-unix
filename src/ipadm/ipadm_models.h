// IP interface/address data models and related helpers.
#pragma once

#include "ipadm_common.h"

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
std::string FormatSockAddrCIDR(const SOCKADDR* sa, UINT8 prefixLen);

bool IsSyntheticWindowsInterface(const std::string& friendlyName, ULONG ifType);
