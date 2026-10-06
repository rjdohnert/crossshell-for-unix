#include "socket_row.hpp"
#include "socket_statistics_collector.hpp"

std::wstring SocketStatisticsCollector::TcpStateToString(DWORD state) {
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

std::wstring SocketStatisticsCollector::FormatIPv4AndPort(DWORD addr, DWORD port) {
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

std::wstring SocketStatisticsCollector::FormatIPv6AndPort(const UCHAR addr[16], DWORD scopeId, DWORD port) {
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

bool SocketStatisticsCollector::CollectTcpRows(std::vector<SocketRow>& rows, bool listeningOnly) {
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

bool SocketStatisticsCollector::CollectUdpRows(std::vector<SocketRow>& rows, bool listeningOnly) {
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
