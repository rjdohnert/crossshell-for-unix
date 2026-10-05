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
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <psapi.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <algorithm>
#include <unordered_map>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")

// ============================================================================
// 1. DATA MODELS & RAII HANDLERS
// ============================================================================

enum class ProtocolFilter {
    ALL,
    TCP_ONLY,
    UDP_ONLY
};

enum class IpFilter {
    ALL,
    IPV4_ONLY,
    IPV6_ONLY
};

struct ConnectionEntry {
    std::string proto;
    std::string localAddr;
    std::string foreignAddr;
    std::string state;
    DWORD pid = 0;
    std::string processName;
};

class WinsockScope {
public:
    WinsockScope() : m_initialized(false) {
        WSADATA wsaData = {};
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            m_initialized = true;
        }
    }

    ~WinsockScope() {
        if (m_initialized) {
            WSACleanup();
        }
    }

    bool IsInitialized() const { return m_initialized; }

private:
    bool m_initialized;
};

class ScopedProcessHandle {
public:
    explicit ScopedProcessHandle(HANDLE handle = nullptr) : m_handle(handle) {}

    ~ScopedProcessHandle() {
        Close();
    }

    ScopedProcessHandle(const ScopedProcessHandle&) = delete;
    ScopedProcessHandle& operator=(const ScopedProcessHandle&) = delete;

    ScopedProcessHandle(ScopedProcessHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }

    ScopedProcessHandle& operator=(ScopedProcessHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE; }

    void Close() {
        if (m_handle && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = nullptr;
        }
    }

private:
    HANDLE m_handle;
};

// ============================================================================
// 2. PROCESS RESOLVER & ADAPTER ENGINE
// ============================================================================

class ProcessResolver {
public:
    static std::string GetProcessNameFromPid(DWORD pid) {
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
};

class NetworkAddressFormatter {
public:
    static std::string GetTcpStateString(DWORD state) {
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

    static std::string FormatIpv4(DWORD ip, DWORD port) {
        in_addr addr;
        addr.S_un.S_addr = ip;
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr, ipStr, sizeof(ipStr));

        std::ostringstream ss;
        ss << ipStr << ":" << ntohs(static_cast<u_short>(port));
        return ss.str();
    }

    static std::string FormatIpv6(const UCHAR* ip, DWORD port, DWORD scopeId = 0) {
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
};

// ============================================================================
// 3. OPTIONS & CONFIGURATION
// ============================================================================

class PortmapOptions {
public:
    bool showAll = false;
    bool numeric = false;
    bool showProcess = true;
    ProtocolFilter proto = ProtocolFilter::ALL;
    IpFilter ipVer = IpFilter::ALL;
    std::string stateFilter = "";
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";
            if (arg == "-h" || arg == "--help" || arg == "/?") {
                showHelp = true;
                return true;
            } else if (arg == "-V" || arg == "--version") {
                showVersion = true;
                return true;
            } else if (arg == "-a" || arg == "--all") {
                showAll = true;
            } else if (arg == "-t" || arg == "--tcp") {
                proto = ProtocolFilter::TCP_ONLY;
            } else if (arg == "-u" || arg == "--udp") {
                proto = ProtocolFilter::UDP_ONLY;
            } else if (arg == "-4" || arg == "--ipv4") {
                ipVer = IpFilter::IPV4_ONLY;
            } else if (arg == "-6" || arg == "--ipv6") {
                ipVer = IpFilter::IPV6_ONLY;
            } else if (arg == "-n" || arg == "--no-proc") {
                showProcess = false;
            } else if ((arg == "-s" || arg == "--state") && i + 1 < argc) {
                std::string state = argv[++i];
                std::transform(state.begin(), state.end(), state.begin(), ::toupper);
                stateFilter = state;
            } else {
                std::cerr << "portmap: unknown option '" << arg << "'\n";
                return false;
            }
        }
        return true;
    }

    void PrintHelp() const {
        std::cout << R"(portmap(1)              CrossShell for UNIX Reference Manual               portmap(1)

    NAME
        portmap - display RPC and active TCP/UDP listening port mappings

    SYNOPSIS
        portmap [OPTIONS]

    DESCRIPTION
        portmap queries and displays all active listening network endpoints,
        bound RPC programs, and associated owning process names.

    OPTIONS
        -a, --all
            Display all endpoints including established connections.

        -t, --tcp
            Filter to TCP endpoints only.

        -u, --udp
            Filter to UDP endpoints only.

        -4, --ipv4
            Filter to IPv4 endpoints.

        -6, --ipv6
            Filter to IPv6 endpoints.

        -n, --no-proc
            Do not resolve process names/PIDs.

        -s, --state STATE
            Filter by state (e.g. LISTENING, ESTABLISHED).

        --json, --csv, --table
            Format port map records as JSON, CSV, or table.

        --pipe COMMAND
            Stream results into COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        portmap -t -s LISTENING
            List all listening TCP ports.

    CrossShell for UNIX                                                portmap(1)
)";
    }

    void PrintVersion() const {
        std::cout << "portmap 2.6.0\n";
    }
};

// ============================================================================
// 4. PORT TABLE COLLECTOR
// ============================================================================

class PortTableCollector {
public:
    static void CollectAll(std::vector<ConnectionEntry>& entries, const PortmapOptions& cfg) {
        CollectTcp4(entries, cfg);
        CollectTcp6(entries, cfg);
        CollectUdp4(entries, cfg);
        CollectUdp6(entries, cfg);
    }

private:
    static void CollectTcp4(std::vector<ConnectionEntry>& entries, const PortmapOptions& cfg) {
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

    static void CollectTcp6(std::vector<ConnectionEntry>& entries, const PortmapOptions& cfg) {
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

    static void CollectUdp4(std::vector<ConnectionEntry>& entries, const PortmapOptions& cfg) {
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

    static void CollectUdp6(std::vector<ConnectionEntry>& entries, const PortmapOptions& cfg) {
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
};

// ============================================================================
// 5. OUTPUT REPORTER
// ============================================================================

class PortmapReporter {
public:
    static void Emit(const PortmapOptions& cfg, const std::vector<ConnectionEntry>& entries) {
        std::cout << "\n"
                  << std::left
                  << std::setw(8)  << "Proto"
                  << std::setw(26) << "Local Address"
                  << std::setw(26) << "Foreign Address"
                  << std::setw(15) << "State";
        if (cfg.showProcess) {
            std::cout << std::setw(9)  << "PID"
                      << "Process Name";
        }
        std::cout << "\n";

        std::cout << std::string(cfg.showProcess ? 105 : 75, '-') << "\n";

        for (const auto& entry : entries) {
            std::cout << std::left
                      << std::setw(8)  << entry.proto
                      << std::setw(26) << entry.localAddr
                      << std::setw(26) << entry.foreignAddr
                      << std::setw(15) << (entry.state.empty() ? "-" : entry.state);

            if (cfg.showProcess) {
                std::cout << std::setw(9)  << entry.pid
                          << entry.processName;
            }
            std::cout << "\n";
        }

        std::cout << std::string(cfg.showProcess ? 105 : 75, '-') << "\n";
        std::cout << "Total active entries listed: " << entries.size() << "\n\n";
    }
};

// ============================================================================
// 6. APPLICATION CONTROLLER
// ============================================================================

class PortmapApplication {
public:
    int Run(int argc, char* argv[]) {
        WinsockScope winsock;
        if (!winsock.IsInitialized()) {
            std::cerr << "[-] Error: Failed to initialize Winsock.\n";
            return 1;
        }

        PortmapOptions cfg;
        if (!cfg.Parse(argc, argv)) {
            std::cerr << "Use 'portmap --help' for usage.\n";
            return 1;
        }

        if (cfg.showHelp) {
            cfg.PrintHelp();
            return 0;
        }

        if (cfg.showVersion) {
            cfg.PrintVersion();
            return 0;
        }

        std::vector<ConnectionEntry> entries;
        PortTableCollector::CollectAll(entries, cfg);
        PortmapReporter::Emit(cfg, entries);

        return 0;
    }
};

int main(int argc, char* argv[]) {
    PortmapApplication app;
    return app.Run(argc, argv);
}