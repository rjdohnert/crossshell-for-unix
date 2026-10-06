#include "engine.hpp"

// ============================================================================
// WinsockScope & ScopedProcessHandle
// ============================================================================

WinsockScope::WinsockScope() : m_initialized(false) {
    WSADATA wsaData = {};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        m_initialized = true;
    }
}

WinsockScope::~WinsockScope() {
    if (m_initialized) {
        WSACleanup();
    }
}

ScopedProcessHandle::ScopedProcessHandle(HANDLE handle) : m_handle(handle) {}

ScopedProcessHandle::~ScopedProcessHandle() {
    Close();
}

ScopedProcessHandle::ScopedProcessHandle(ScopedProcessHandle&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = nullptr;
}

ScopedProcessHandle& ScopedProcessHandle::operator=(ScopedProcessHandle&& other) noexcept {
    if (this != &other) {
        Close();
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

void ScopedProcessHandle::Close() {
    if (m_handle && m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_handle);
        m_handle = nullptr;
    }
}

// ============================================================================
// ProcessResolver
// ============================================================================

std::string ProcessResolver::GetProcessNameFromPid(DWORD pid) {
    if (pid == 0) return "System Idle Process";
    if (pid == 4) return "System";

    ScopedProcessHandle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!hProcess.IsValid()) {
        return "<Unknown/Access Denied>";
    }

    char buffer[MAX_PATH];
    DWORD size = MAX_PATH;
    std::string processName = "<Unknown>";

    if (QueryFullProcessImageNameA(hProcess.Get(), 0, buffer, &size)) {
        std::string fullPath(buffer);
        size_t lastSlash = fullPath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            processName = fullPath.substr(lastSlash + 1);
        } else {
            processName = fullPath;
        }
    }
    return processName;
}

// ============================================================================
// NetworkAddressFormatter
// ============================================================================

std::string NetworkAddressFormatter::GetTcpStateString(DWORD state) {
    switch (state) {
        case MIB_TCP_STATE_CLOSED:     return "CLOSED";
        case MIB_TCP_STATE_LISTEN:     return "LISTENING";
        case MIB_TCP_STATE_SYN_SENT:   return "SYN_SENT";
        case MIB_TCP_STATE_SYN_RCVD:   return "SYN_RECEIVED";
        case MIB_TCP_STATE_ESTAB:      return "ESTABLISHED";
        case MIB_TCP_STATE_FIN_WAIT1:  return "FIN_WAIT_1";
        case MIB_TCP_STATE_FIN_WAIT2:  return "FIN_WAIT_2";
        case MIB_TCP_STATE_CLOSE_WAIT: return "CLOSE_WAIT";
        case MIB_TCP_STATE_CLOSING:    return "CLOSING";
        case MIB_TCP_STATE_LAST_ACK:   return "LAST_ACK";
        case MIB_TCP_STATE_TIME_WAIT:  return "TIME_WAIT";
        case MIB_TCP_STATE_DELETE_TCB: return "DELETE_TCB";
        default:                       return "UNKNOWN";
    }
}

std::string NetworkAddressFormatter::FormatIpv4(DWORD ip, DWORD port) {
    in_addr addr;
    addr.S_un.S_addr = ip;
    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, ipStr, sizeof(ipStr));

    std::ostringstream ss;
    ss << ipStr << ":" << ntohs(static_cast<u_short>(port));
    return ss.str();
}

std::string NetworkAddressFormatter::FormatIpv6(const UCHAR* ip, DWORD port, DWORD scopeId) {
    in6_addr addr;
    std::memcpy(&addr, ip, sizeof(in6_addr));
    char ipStr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &addr, ipStr, sizeof(ipStr));

    std::ostringstream ss;
    if (scopeId != 0) {
        ss << "[" << ipStr << "%" << scopeId << "]:" << ntohs(static_cast<u_short>(port));
    } else {
        ss << "[" << ipStr << "]:" << ntohs(static_cast<u_short>(port));
    }
    return ss.str();
}

// ============================================================================
// PortTableCollector
// ============================================================================

void PortTableCollector::CollectAll(std::vector<ConnectionEntry>& entries, const PortmapOptions& cfg) {
    CollectTcp4(entries, cfg);
    CollectTcp6(entries, cfg);
    CollectUdp4(entries, cfg);
    CollectUdp6(entries, cfg);
}

void PortTableCollector::CollectTcp4(std::vector<ConnectionEntry>& entries, const PortmapOptions& cfg) {
    if (cfg.proto == ProtocolFilter::UDP_ONLY || cfg.ipVer == IpFilter::IPV6_ONLY) return;

    DWORD size = 0;
    GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (size == 0) return;

    std::vector<BYTE> buffer(size);
    if (GetExtendedTcpTable(buffer.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
        auto* table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buffer.data());
        for (DWORD i = 0; i < table->dwNumEntries; ++i) {
            const auto& row = table->table[i];
            std::string state = NetworkAddressFormatter::GetTcpStateString(row.dwState);

            if (!cfg.showAll && row.dwState == MIB_TCP_STATE_LISTEN) continue;
            if (!cfg.stateFilter.empty() && state != cfg.stateFilter) continue;

            ConnectionEntry entry;
            entry.proto = "TCPv4";
            entry.localAddr = NetworkAddressFormatter::FormatIpv4(row.dwLocalAddr, row.dwLocalPort);
            entry.foreignAddr = (row.dwRemoteAddr == 0) ? "*:*" : NetworkAddressFormatter::FormatIpv4(row.dwRemoteAddr, row.dwRemotePort);
            entry.state = state;
            entry.pid = row.dwOwningPid;
            if (cfg.showProcess) entry.processName = ProcessResolver::GetProcessNameFromPid(row.dwOwningPid);
            entries.push_back(entry);
        }
    }
}

void PortTableCollector::CollectTcp6(std::vector<ConnectionEntry>& entries, const PortmapOptions& cfg) {
    if (cfg.proto == ProtocolFilter::UDP_ONLY || cfg.ipVer == IpFilter::IPV4_ONLY) return;

    DWORD size = 0;
    GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0);
    if (size == 0) return;

    std::vector<BYTE> buffer(size);
    if (GetExtendedTcpTable(buffer.data(), &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
        auto* table = reinterpret_cast<MIB_TCP6TABLE_OWNER_PID*>(buffer.data());
        for (DWORD i = 0; i < table->dwNumEntries; ++i) {
            const auto& row = table->table[i];
            std::string state = NetworkAddressFormatter::GetTcpStateString(row.dwState);

            if (!cfg.showAll && row.dwState == MIB_TCP_STATE_LISTEN) continue;
            if (!cfg.stateFilter.empty() && state != cfg.stateFilter) continue;

            ConnectionEntry entry;
            entry.proto = "TCPv6";
            entry.localAddr = NetworkAddressFormatter::FormatIpv6(row.ucLocalAddr, row.dwLocalPort, row.dwLocalScopeId);
            entry.foreignAddr = NetworkAddressFormatter::FormatIpv6(row.ucRemoteAddr, row.dwRemotePort, row.dwRemoteScopeId);
            entry.state = state;
            entry.pid = row.dwOwningPid;
            if (cfg.showProcess) entry.processName = ProcessResolver::GetProcessNameFromPid(row.dwOwningPid);
            entries.push_back(entry);
        }
    }
}

void PortTableCollector::CollectUdp4(std::vector<ConnectionEntry>& entries, const PortmapOptions& cfg) {
    if (cfg.proto == ProtocolFilter::TCP_ONLY || cfg.ipVer == IpFilter::IPV6_ONLY) return;
    if (!cfg.stateFilter.empty()) return;

    DWORD size = 0;
    GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    if (size == 0) return;

    std::vector<BYTE> buffer(size);
    if (GetExtendedUdpTable(buffer.data(), &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
        auto* table = reinterpret_cast<MIB_UDPTABLE_OWNER_PID*>(buffer.data());
        for (DWORD i = 0; i < table->dwNumEntries; ++i) {
            const auto& row = table->table[i];
            ConnectionEntry entry;
            entry.proto = "UDPv4";
            entry.localAddr = NetworkAddressFormatter::FormatIpv4(row.dwLocalAddr, row.dwLocalPort);
            entry.foreignAddr = "*:*";
            entry.state = "";
            entry.pid = row.dwOwningPid;
            if (cfg.showProcess) entry.processName = ProcessResolver::GetProcessNameFromPid(row.dwOwningPid);
            entries.push_back(entry);
        }
    }
}

void PortTableCollector::CollectUdp6(std::vector<ConnectionEntry>& entries, const PortmapOptions& cfg) {
    if (cfg.proto == ProtocolFilter::TCP_ONLY || cfg.ipVer == IpFilter::IPV4_ONLY) return;
    if (!cfg.stateFilter.empty()) return;

    DWORD size = 0;
    GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0);
    if (size == 0) return;

    std::vector<BYTE> buffer(size);
    if (GetExtendedUdpTable(buffer.data(), &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
        auto* table = reinterpret_cast<MIB_UDP6TABLE_OWNER_PID*>(buffer.data());
        for (DWORD i = 0; i < table->dwNumEntries; ++i) {
            const auto& row = table->table[i];
            ConnectionEntry entry;
            entry.proto = "UDPv6";
            entry.localAddr = NetworkAddressFormatter::FormatIpv6(row.ucLocalAddr, row.dwLocalPort, row.dwLocalScopeId);
            entry.foreignAddr = "*:*";
            entry.state = "";
            entry.pid = row.dwOwningPid;
            if (cfg.showProcess) entry.processName = ProcessResolver::GetProcessNameFromPid(row.dwOwningPid);
            entries.push_back(entry);
        }
    }
}
