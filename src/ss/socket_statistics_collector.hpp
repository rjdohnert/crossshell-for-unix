#pragma once

#include "socket_row.hpp"
#include "ss.hpp"

class SocketStatisticsCollector {
public:
    static std::wstring TcpStateToString(DWORD state);

    static std::wstring FormatIPv4AndPort(DWORD addr, DWORD port);

    static std::wstring FormatIPv6AndPort(const UCHAR addr[16], DWORD scopeId, DWORD port);

    static bool CollectTcpRows(std::vector<SocketRow>& rows, bool listeningOnly);

    static bool CollectUdpRows(std::vector<SocketRow>& rows, bool listeningOnly);
};
