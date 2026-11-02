// ipadm subcommand implementations.
#include "ipadm_subcommands.h"
#include "ipadm_helpers.h"

// Subcommand: show-if
void ShowIf(const std::vector<IpInterfaceInfo>& interfaces, const std::string& targetIf, const std::string& fmt) {
    if (fmt == "json") {
        std::cout << "[\n";
        bool first = true;
        for (const auto& i : interfaces) {
            if (!targetIf.empty() && i.ifName != targetIf) continue;
            if (!first) std::cout << ",\n";
            first = false;
            std::cout << "  {\"ifname\":\"" << JsonEscape(i.ifName)
                      << "\",\"class\":\"" << JsonEscape(i.ifClass)
                      << "\",\"state\":\"" << JsonEscape(i.state)
                      << "\",\"active\":\"" << JsonEscape(i.active)
                      << "\",\"over\":\"" << JsonEscape(i.overLink) << "\"}";
        }
        std::cout << "\n]\n";
        return;
    }
    if (fmt == "csv") {
        std::cout << "IFNAME,CLASS,STATE,ACTIVE,OVER\n";
        for (const auto& i : interfaces) {
            if (!targetIf.empty() && i.ifName != targetIf) continue;
            std::cout << CsvEscape(i.ifName) << "," << CsvEscape(i.ifClass)
                      << "," << CsvEscape(i.state) << "," << CsvEscape(i.active)
                      << "," << CsvEscape(i.overLink) << "\n";
        }
        return;
    }

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
void ShowAddr(const std::vector<IpAddressObject>& addrs, const std::string& targetAddrObj, const std::string& fmt) {
    if (fmt == "json") {
        std::cout << "[\n";
        bool first = true;
        for (const auto& a : addrs) {
            if (!targetAddrObj.empty() && a.addrobj != targetAddrObj && a.ifName != targetAddrObj) continue;
            if (!first) std::cout << ",\n";
            first = false;
            std::cout << "  {\"addrobj\":\"" << JsonEscape(a.addrobj)
                      << "\",\"type\":\"" << JsonEscape(a.type)
                      << "\",\"state\":\"" << JsonEscape(a.state)
                      << "\",\"addr\":\"" << JsonEscape(a.ipAddress) << "\"}";
        }
        std::cout << "\n]\n";
        return;
    }
    if (fmt == "csv") {
        std::cout << "ADDROBJ,TYPE,STATE,ADDR\n";
        for (const auto& a : addrs) {
            if (!targetAddrObj.empty() && a.addrobj != targetAddrObj && a.ifName != targetAddrObj) continue;
            std::cout << CsvEscape(a.addrobj) << "," << CsvEscape(a.type)
                      << "," << CsvEscape(a.state) << "," << CsvEscape(a.ipAddress) << "\n";
        }
        return;
    }

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
    UINT8 prefixLen = 24;
    if (slashPos != std::string::npos) {
        try {
            int p = std::stoi(cidrAddr.substr(slashPos + 1));
            if (p < 0 || p > 128) {
                std::cerr << "[-] Error: invalid CIDR prefix in '" << cidrAddr << "'\n";
                return false;
            }
            prefixLen = static_cast<UINT8>(p);
        } catch (...) {
            std::cerr << "[-] Error: invalid CIDR prefix in '" << cidrAddr << "'\n";
            return false;
        }
    }

    // Validate the IP address before committing.
    IN_ADDR testAddr;
    if (inet_pton(AF_INET, ipOnly.c_str(), &testAddr) != 1) {
        std::cerr << "[-] Error: invalid IPv4 address '" << ipOnly << "'\n";
        return false;
    }

    MIB_UNICASTIPADDRESS_ROW row;
    InitializeUnicastIpAddressEntry(&row);
    row.InterfaceIndex = targetIndex;
    row.OnLinkPrefixLength = prefixLen;

    SOCKADDR_IN* sin = reinterpret_cast<SOCKADDR_IN*>(&row.Address);
    sin->sin_family = AF_INET;
    sin->sin_addr = testAddr;

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
