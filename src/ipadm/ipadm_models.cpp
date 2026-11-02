// IP interface/address data models and related helpers.
#include "ipadm_models.h"

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
        text.find("ras") != std::string::npos;
    // Note: do NOT filter on "microsoft" -- a Microsoft-branded physical NIC is
    // a legitimate adapter, not a synthetic interface.

    return synthetic;
}
