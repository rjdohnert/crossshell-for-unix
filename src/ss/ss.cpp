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

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <memory>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

// ============================================================================
// 1. RAII SCOPES & DATA MODELS
// ============================================================================

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

struct SocketRow {
    std::wstring proto;
    std::wstring local;
    std::wstring peer;
    std::wstring state;
    DWORD pid = 0;
};

// ============================================================================
// 2. SOCKET STATISTICS COLLECTOR
// ============================================================================

class SocketStatisticsCollector {
public:
    static std::wstring TcpStateToString(DWORD state) {
        switch (state) {
            case MIB_TCP_STATE_CLOSED: return L"CLOSED";
            case MIB_TCP_STATE_LISTEN: return L"LISTEN";
            case MIB_TCP_STATE_SYN_SENT: return L"SYN-SENT";
            case MIB_TCP_STATE_SYN_RCVD: return L"SYN-RECV";
            case MIB_TCP_STATE_ESTAB: return L"ESTAB";
            case MIB_TCP_STATE_FIN_WAIT1: return L"FIN-WAIT-1";
            case MIB_TCP_STATE_FIN_WAIT2: return L"FIN-WAIT-2";
            case MIB_TCP_STATE_CLOSE_WAIT: return L"CLOSE-WAIT";
            case MIB_TCP_STATE_CLOSING: return L"CLOSING";
            case MIB_TCP_STATE_LAST_ACK: return L"LAST-ACK";
            case MIB_TCP_STATE_TIME_WAIT: return L"TIME-WAIT";
            case MIB_TCP_STATE_DELETE_TCB: return L"DELETE-TCB";
            default: return L"UNKNOWN";
        }
    }

    static std::wstring FormatIPv4AndPort(DWORD addr, DWORD port) {
        IN_ADDR inAddr = {};
        inAddr.S_un.S_addr = addr;
        wchar_t ip[64] = {};
        if (!InetNtopW(AF_INET, &inAddr, ip, 64)) {
            wcscpy_s(ip, L"0.0.0.0");
        }

        unsigned short hostPort = ntohs(static_cast<u_short>(port));
        std::wstring result = ip;
        result += L":";
        result += std::to_wstring(hostPort);
        return result;
    }

    static std::wstring FormatIPv6AndPort(const UCHAR addr[16], DWORD scopeId, DWORD port) {
        IN6_ADDR in6 = {};
        memcpy(&in6, addr, 16);
        wchar_t ip[128] = {};
        if (!InetNtopW(AF_INET6, &in6, ip, 128)) {
            wcscpy_s(ip, L"::");
        }

        unsigned short hostPort = ntohs(static_cast<u_short>(port));
        std::wstring result = L"[";
        result += ip;
        if (scopeId != 0) {
            result += L"%";
            result += std::to_wstring(scopeId);
        }
        result += L"]:";
        result += std::to_wstring(hostPort);
        return result;
    }

    static bool CollectTcpRows(std::vector<SocketRow>& rows, bool listeningOnly) {
        DWORD size = 0;
        GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
        if (size > 0) {
            std::vector<BYTE> buf(size);
            if (GetExtendedTcpTable(buf.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
                auto* table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buf.data());
                for (DWORD i = 0; i < table->dwNumEntries; ++i) {
                    const auto& r = table->table[i];
                    if (listeningOnly && r.dwState != MIB_TCP_STATE_LISTEN) continue;

                    SocketRow row;
                    row.proto = L"tcp";
                    row.local = FormatIPv4AndPort(r.dwLocalAddr, r.dwLocalPort);
                    row.peer = FormatIPv4AndPort(r.dwRemoteAddr, r.dwRemotePort);
                    row.state = TcpStateToString(r.dwState);
                    row.pid = r.dwOwningPid;
                    rows.push_back(row);
                }
            }
        }

        size = 0;
        GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0);
        if (size > 0) {
            std::vector<BYTE> buf6(size);
            if (GetExtendedTcpTable(buf6.data(), &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
                auto* table6 = reinterpret_cast<PMIB_TCP6TABLE_OWNER_PID>(buf6.data());
                for (DWORD i = 0; i < table6->dwNumEntries; ++i) {
                    const auto& r = table6->table[i];
                    if (listeningOnly && r.dwState != MIB_TCP_STATE_LISTEN) continue;

                    SocketRow row;
                    row.proto = L"tcp6";
                    row.local = FormatIPv6AndPort(r.ucLocalAddr, r.dwLocalScopeId, r.dwLocalPort);
                    row.peer = FormatIPv6AndPort(r.ucRemoteAddr, r.dwRemoteScopeId, r.dwRemotePort);
                    row.state = TcpStateToString(r.dwState);
                    row.pid = r.dwOwningPid;
                    rows.push_back(row);
                }
            }
        }

        return true;
    }

    static bool CollectUdpRows(std::vector<SocketRow>& rows, bool listeningOnly) {
        (void)listeningOnly;
        DWORD size = 0;
        GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
        if (size > 0) {
            std::vector<BYTE> buf(size);
            if (GetExtendedUdpTable(buf.data(), &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
                auto* table = reinterpret_cast<PMIB_UDPTABLE_OWNER_PID>(buf.data());
                for (DWORD i = 0; i < table->dwNumEntries; ++i) {
                    const auto& r = table->table[i];
                    SocketRow row;
                    row.proto = L"udp";
                    row.local = FormatIPv4AndPort(r.dwLocalAddr, r.dwLocalPort);
                    row.peer = L"*:*";
                    row.state = L"UNCONN";
                    row.pid = r.dwOwningPid;
                    rows.push_back(row);
                }
            }
        }

        size = 0;
        GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0);
        if (size > 0) {
            std::vector<BYTE> buf6(size);
            if (GetExtendedUdpTable(buf6.data(), &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
                auto* table6 = reinterpret_cast<PMIB_UDP6TABLE_OWNER_PID>(buf6.data());
                for (DWORD i = 0; i < table6->dwNumEntries; ++i) {
                    const auto& r = table6->table[i];
                    SocketRow row;
                    row.proto = L"udp6";
                    row.local = FormatIPv6AndPort(r.ucLocalAddr, r.dwLocalScopeId, r.dwLocalPort);
                    row.peer = L"*:*";
                    row.state = L"UNCONN";
                    row.pid = r.dwOwningPid;
                    rows.push_back(row);
                }
            }
        }

        return true;
    }
};

// ============================================================================
// 3. OPTIONS & COMMAND LINE PARSER
// ============================================================================

class SsOptions {
public:
    bool showTcp = true;
    bool showUdp = true;
    bool listeningOnly = false;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]) {
        bool modeSpecified = false;
        showTcp = true;
        showUdp = true;

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";

            if (arg == L"-h" || arg == L"--help") {
                showHelp = true;
                return true;
            }
            if (arg == L"--version" || arg == L"-V") {
                showVersion = true;
                return true;
            }
            if (arg == L"-n" || arg == L"--numeric") {
                continue;
            }
            if (arg == L"-l" || arg == L"--listening") {
                listeningOnly = true;
                continue;
            }
            if (arg == L"-t" || arg == L"--tcp") {
                if (!modeSpecified) {
                    showTcp = false;
                    showUdp = false;
                    modeSpecified = true;
                }
                showTcp = true;
                continue;
            }
            if (arg == L"-u" || arg == L"--udp") {
                if (!modeSpecified) {
                    showTcp = false;
                    showUdp = false;
                    modeSpecified = true;
                }
                showUdp = true;
                continue;
            }

            std::wcerr << L"ss: unknown option -- " << arg << L"\n";
            return false;
        }

        return true;
    }

    void PrintUsage(const wchar_t* programName) const {
        std::wcout << LR"(ss(1)                   CrossShell for UNIX Reference Manual                    ss(1)

    NAME
        ss - dump network socket statistics and active endpoints

    SYNOPSIS
        ss [OPTIONS] [FILTER]

    DESCRIPTION
        ss is used to dump socket statistics. It allows showing information
        similar to netstat, displaying active TCP, UDP, and raw sockets.

    OPTIONS
        -t, --tcp
            Display TCP sockets.

        -u, --udp
            Display UDP sockets.

        -l, --listening
            Display only listening sockets.

        -a, --all
            Display both listening and non-listening sockets.

        -n, --numeric
            Do not try to resolve service names or hostnames.

        -p, --processes
            Show process using socket.

        --json, --csv, --table
            Format socket records as JSON, CSV, or table.

        --pipe COMMAND
            Send output directly to COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        ss -t -a
            Display all TCP sockets.

        ss -l -n --json
            Display listening sockets numerically in JSON format.

    CrossShell for UNIX                                                     ss(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"ss 1.0.0\n";
    }
};

// ============================================================================
// 4. OUTPUT REPORTER
// ============================================================================

class SsReporter {
public:
    static void Emit(const std::vector<SocketRow>& rows) {
        std::wcout << std::left
                   << std::setw(7) << L"Netid"
                   << std::setw(34) << L"Local Address:Port"
                   << std::setw(34) << L"Peer Address:Port"
                   << std::setw(13) << L"State"
                   << L"PID\n";

        for (const auto& row : rows) {
            std::wcout << std::left
                       << std::setw(7) << row.proto
                       << std::setw(34) << row.local
                       << std::setw(34) << row.peer
                       << std::setw(13) << row.state
                       << row.pid << L"\n";
        }
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class SsApplication {
public:
    int Run(int argc, wchar_t* argv[]) {
        WinsockScope winsock;
        if (!winsock.IsInitialized()) {
            std::wcerr << L"ss: failed to initialize Winsock\n";
            return 1;
        }

        SsOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintUsage(argc > 0 ? argv[0] : L"ss");
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage(argc > 0 ? argv[0] : L"ss");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        std::vector<SocketRow> rows;
        bool ok = true;
        if (opts.showTcp) ok = SocketStatisticsCollector::CollectTcpRows(rows, opts.listeningOnly) && ok;
        if (opts.showUdp) ok = SocketStatisticsCollector::CollectUdpRows(rows, opts.listeningOnly) && ok;

        std::sort(rows.begin(), rows.end(), [](const SocketRow& a, const SocketRow& b) {
            if (a.proto != b.proto) return a.proto < b.proto;
            if (a.local != b.local) return a.local < b.local;
            return a.pid < b.pid;
        });

        SsReporter::Emit(rows);

        if (!ok) {
            std::wcerr << L"ss: warning: some socket tables could not be read\n";
        }

        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    SsApplication app;
    return app.Run(argc, argv);
}

