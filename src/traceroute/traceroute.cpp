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

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <algorithm>
#include <clocale>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")

// ============================================================================
// 1. DATA MODELS & RAII SCOPES
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

class ScopedSocket {
public:
    explicit ScopedSocket(SOCKET sock = INVALID_SOCKET) : m_socket(sock) {}

    ~ScopedSocket() {
        Close();
    }

    ScopedSocket(const ScopedSocket&) = delete;
    ScopedSocket& operator=(const ScopedSocket&) = delete;

    ScopedSocket(ScopedSocket&& other) noexcept : m_socket(other.m_socket) {
        other.m_socket = INVALID_SOCKET;
    }

    ScopedSocket& operator=(ScopedSocket&& other) noexcept {
        if (this != &other) {
            Close();
            m_socket = other.m_socket;
            other.m_socket = INVALID_SOCKET;
        }
        return *this;
    }

    SOCKET Get() const { return m_socket; }
    bool IsValid() const { return m_socket != INVALID_SOCKET; }

    void Close() {
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
    }

    void Reset(SOCKET s = INVALID_SOCKET) {
        Close();
        m_socket = s;
    }

private:
    SOCKET m_socket;
};

#pragma pack(push, 1)
struct IPHeader {
    BYTE  ver_len;       // Version (4 bits) + Header length (4 bits)
    BYTE  tos;           // Type of service
    WORD  total_len;     // Total length
    WORD  id;            // Identification
    WORD  flags_offset;  // Flags + Fragment offset
    BYTE  ttl;           // Time to live
    BYTE  protocol;      // Protocol
    WORD  checksum;      // Checksum
    IN_ADDR src_addr;    // Source address
    IN_ADDR dst_addr;    // Destination address
};

struct ICMPHeader {
    BYTE type;           // ICMP Type
    BYTE code;           // ICMP Code
    WORD checksum;       // ICMP Checksum
    WORD id;             // ICMP ID
    WORD sequence;       // ICMP Sequence
};

struct UDPHeader {
    WORD src_port;       // Source port
    WORD dst_port;       // Destination port
    WORD length;         // UDP length
    WORD checksum;       // UDP checksum
};
#pragma pack(pop)

struct ProbeResult {
    bool responded = false;
    double rttMs = 0.0;
    IN_ADDR responderIp = {};
    BYTE icmpType = 0;
    BYTE icmpCode = 0;
};

// ============================================================================
// 2. NETWORK UTILITIES & RESOLVER
// ============================================================================

class NetworkUtils {
public:
    static bool IsUserAdmin() {
        BOOL isAdmin = FALSE;
        PSID adminGroup = NULL;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
            CheckTokenMembership(NULL, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }
        return isAdmin == TRUE;
    }

    static WORD CalculateChecksum(WORD* buffer, int size) {
        DWORD cksum = 0;
        while (size > 1) {
            cksum += *buffer++;
            size -= sizeof(WORD);
        }
        if (size) {
            cksum += *(BYTE*)buffer;
        }
        cksum = (cksum >> 16) + (cksum & 0xffff);
        cksum += (cksum >> 16);
        return (WORD)(~cksum);
    }

    static std::string ResolveHostname(IN_ADDR addr, bool numericMode) {
        char ipStr[INET_ADDRSTRLEN] = { 0 };
        inet_ntop(AF_INET, &addr, ipStr, INET_ADDRSTRLEN);

        if (numericMode) return std::string(ipStr);

        sockaddr_in sa = {};
        sa.sin_family = AF_INET;
        sa.sin_addr = addr;

        char hostBuf[NI_MAXHOST] = { 0 };
        if (getnameinfo((sockaddr*)&sa, sizeof(sa), hostBuf, sizeof(hostBuf), NULL, 0, NI_NAMEREQD) == 0) {
            return std::string(hostBuf) + " (" + std::string(ipStr) + ")";
        }
        return std::string(ipStr);
    }
};

// ============================================================================
// 3. OPTIONS & COMMAND LINE PARSER
// ============================================================================

class TracerouteOptions {
public:
    int maxTtl = 30;
    int firstTtl = 1;
    int basePort = 33434;
    int nQueries = 3;
    int waitTimeSec = 3;
    bool numericMode = false;
    bool icmpMode = false;
    std::string srcIpStr;
    std::string targetHostStr;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";
            if (arg == "-?" || arg == "-h" || arg == "--help") {
                showHelp = true;
                return true;
            } else if (arg == "-V" || arg == "--version") {
                showVersion = true;
                return true;
            } else if (arg == "-n") {
                numericMode = true;
            } else if (arg == "-I") {
                icmpMode = true;
            } else if (arg == "-m" && i + 1 < argc) {
                maxTtl = std::atoi(argv[++i]);
            } else if (arg == "-f" && i + 1 < argc) {
                firstTtl = std::atoi(argv[++i]);
            } else if (arg == "-p" && i + 1 < argc) {
                basePort = std::atoi(argv[++i]);
            } else if (arg == "-q" && i + 1 < argc) {
                nQueries = std::atoi(argv[++i]);
            } else if (arg == "-w" && i + 1 < argc) {
                waitTimeSec = std::atoi(argv[++i]);
            } else if (arg == "-s" && i + 1 < argc) {
                srcIpStr = argv[++i];
            } else if (!arg.empty() && arg[0] != '-') {
                targetHostStr = arg;
            } else {
                std::cerr << "traceroute: unknown option '" << arg << "'\n";
                return false;
            }
        }
        return true;
    }

    void PrintHelp(const char* progName) const {
        std::cout << R"(traceroute(1)           CrossShell for UNIX Reference Manual           traceroute(1)

    NAME
        traceroute - print the route packets trace to network host

    SYNOPSIS
        traceroute [OPTIONS] HOST

    DESCRIPTION
        traceroute tracks the route packets follow across an IP network on their
        way to a given host using ICMP or UDP probe packets with incrementing TTL.

    OPTIONS
        -I
            Use ICMP ECHO for probes instead of UDP.

        -n
            Do not resolve IP addresses to hostnames.

        -m MAX_TTL
            Set maximum number of hops (TTL value) to probe.

        -w SECONDS
            Set wait time for probe response.

        -p PORT
            Set base destination port for UDP probes.

        -q NQUERIES
            Set number of probe packets per hop.

        --json, --csv, --table
            Output hop records as JSON, CSV, or table.

        --pipe COMMAND
            Stream hop telemetry to COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        traceroute -I google.com
            Trace route using ICMP ECHO probes.

    CrossShell for UNIX                                            traceroute(1)
)";
    }

    void PrintVersion() const {
        std::cout << "traceroute 1.0.0\n";
    }
};

// ============================================================================
// 4. TRACEROUTE ENGINE
// ============================================================================

class TracerouteEngine {
public:
    explicit TracerouteEngine(const TracerouteOptions& opts) : m_opts(opts) {}

    bool Execute() {
        addrinfo hints = {}, * res = nullptr;
        hints.ai_family = AF_INET;
        if (getaddrinfo(m_opts.targetHostStr.c_str(), NULL, &hints, &res) != 0 || !res) {
            std::cerr << "traceroute: unknown host " << m_opts.targetHostStr << "\n";
            return false;
        }

        sockaddr_in targetAddr = *(sockaddr_in*)res->ai_addr;
        char targetIpStr[INET_ADDRSTRLEN] = { 0 };
        inet_ntop(AF_INET, &targetAddr.sin_addr, targetIpStr, INET_ADDRSTRLEN);
        freeaddrinfo(res);

        ScopedSocket sendSock(socket(AF_INET, m_opts.icmpMode ? SOCK_RAW : SOCK_DGRAM, m_opts.icmpMode ? IPPROTO_ICMP : IPPROTO_UDP));
        ScopedSocket recvSock(socket(AF_INET, SOCK_RAW, IPPROTO_ICMP));

        if (!sendSock.IsValid() || !recvSock.IsValid()) {
            std::cerr << "traceroute: socket creation failed (Error: " << WSAGetLastError() << "). Check Admin rights.\n";
            return false;
        }

        if (!m_opts.srcIpStr.empty()) {
            sockaddr_in localAddr = {};
            localAddr.sin_family = AF_INET;
            inet_pton(AF_INET, m_opts.srcIpStr.c_str(), &localAddr.sin_addr);
            bind(sendSock.Get(), (sockaddr*)&localAddr, sizeof(localAddr));
        }

        sockaddr_in recvBindAddr = {};
        recvBindAddr.sin_family = AF_INET;
        recvBindAddr.sin_addr.s_addr = INADDR_ANY;
        bind(recvSock.Get(), (sockaddr*)&recvBindAddr, sizeof(recvBindAddr));

        DWORD timeoutMs = m_opts.waitTimeSec * 1000;
        setsockopt(recvSock.Get(), SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));

        int packetSize = m_opts.icmpMode ? (sizeof(ICMPHeader) + 32) : 38;
        std::cout << "traceroute to " << m_opts.targetHostStr << " (" << targetIpStr << "), "
                  << m_opts.maxTtl << " hops max, " << packetSize << " byte packets\n";

        bool destinationReached = false;
        WORD icmpSeq = 0;

        for (int ttl = m_opts.firstTtl; ttl <= m_opts.maxTtl && !destinationReached; ++ttl) {
            std::cout << std::right << std::setw(2) << ttl << "  ";

            setsockopt(sendSock.Get(), IPPROTO_IP, IP_TTL, (const char*)&ttl, sizeof(ttl));

            IN_ADDR lastHopIp = {};
            bool hopIpPrinted = false;

            for (int query = 0; query < m_opts.nQueries; ++query) {
                WORD currentPort = (WORD)(m_opts.basePort + (ttl - 1) * m_opts.nQueries + query);
                targetAddr.sin_port = htons(currentPort);

                ProbeResult result = {};
                auto startTime = std::chrono::high_resolution_clock::now();

                if (m_opts.icmpMode) {
                    char icmpBuf[64] = {};
                    ICMPHeader* icmp = (ICMPHeader*)icmpBuf;
                    icmp->type = 8;
                    icmp->code = 0;
                    icmp->id = (WORD)GetCurrentProcessId();
                    icmp->sequence = htons(++icmpSeq);
                    icmp->checksum = NetworkUtils::CalculateChecksum((WORD*)icmpBuf, sizeof(ICMPHeader) + 32);

                    sendto(sendSock.Get(), icmpBuf, sizeof(ICMPHeader) + 32, 0, (sockaddr*)&targetAddr, sizeof(targetAddr));
                } else {
                    char payload[38] = "SYSTEM TRACEROUTE PROBE PACKET";
                    sendto(sendSock.Get(), payload, sizeof(payload), 0, (sockaddr*)&targetAddr, sizeof(targetAddr));
                }

                char recvBuf[512] = { 0 };
                sockaddr_in fromAddr = {};
                int fromLen = sizeof(fromAddr);

                while (true) {
                    int bytesRecv = recvfrom(recvSock.Get(), recvBuf, sizeof(recvBuf), 0, (sockaddr*)&fromAddr, &fromLen);
                    auto endTime = std::chrono::high_resolution_clock::now();

                    if (bytesRecv == SOCKET_ERROR) {
                        result.responded = false;
                        break;
                    }

                    IPHeader* outerIp = (IPHeader*)recvBuf;
                    int ipHdrLen = (outerIp->ver_len & 0x0F) * 4;
                    ICMPHeader* icmp = (ICMPHeader*)(recvBuf + ipHdrLen);

                    bool validResponse = false;

                    if (icmp->type == 11) {
                        IPHeader* innerIp = (IPHeader*)(recvBuf + ipHdrLen + sizeof(ICMPHeader));
                        int innerIpHdrLen = (innerIp->ver_len & 0x0F) * 4;

                        if (m_opts.icmpMode) {
                            ICMPHeader* innerIcmp = (ICMPHeader*)(recvBuf + ipHdrLen + sizeof(ICMPHeader) + innerIpHdrLen);
                            if (innerIcmp->id == (WORD)GetCurrentProcessId()) validResponse = true;
                        } else {
                            UDPHeader* innerUdp = (UDPHeader*)(recvBuf + ipHdrLen + sizeof(ICMPHeader) + innerIpHdrLen);
                            if (ntohs(innerUdp->dst_port) == currentPort) validResponse = true;
                        }
                    } else if (icmp->type == 3) {
                        IPHeader* innerIp = (IPHeader*)(recvBuf + ipHdrLen + sizeof(ICMPHeader));
                        int innerIpHdrLen = (innerIp->ver_len & 0x0F) * 4;

                        if (!m_opts.icmpMode) {
                            UDPHeader* innerUdp = (UDPHeader*)(recvBuf + ipHdrLen + sizeof(ICMPHeader) + innerIpHdrLen);
                            if (ntohs(innerUdp->dst_port) == currentPort) {
                                validResponse = true;
                                destinationReached = true;
                            }
                        } else {
                            validResponse = true;
                            destinationReached = true;
                        }
                    } else if (m_opts.icmpMode && icmp->type == 0) {
                        if (icmp->id == (WORD)GetCurrentProcessId()) {
                            validResponse = true;
                            destinationReached = true;
                        }
                    }

                    if (validResponse) {
                        result.responded = true;
                        result.rttMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
                        result.responderIp = outerIp->src_addr;
                        result.icmpType = icmp->type;
                        result.icmpCode = icmp->code;
                        break;
                    }
                }

                if (result.responded) {
                    if (!hopIpPrinted || lastHopIp.s_addr != result.responderIp.s_addr) {
                        std::string hostStr = NetworkUtils::ResolveHostname(result.responderIp, m_opts.numericMode);
                        std::cout << hostStr << "  ";
                        lastHopIp = result.responderIp;
                        hopIpPrinted = true;
                    }

                    std::cout << std::fixed << std::setprecision(3) << result.rttMs << " ms";

                    if (result.icmpType == 3) {
                        if (result.icmpCode == 1) std::cout << " !N";
                        else if (result.icmpCode == 2) std::cout << " !P";
                        else if (result.icmpCode == 3) std::cout << " !P";
                        else if (result.icmpCode == 5) std::cout << " !S";
                    }
                    std::cout << "  ";
                } else {
                    std::cout << "* ";
                }
            }
            std::cout << "\n";
        }

        return true;
    }

private:
    const TracerouteOptions& m_opts;
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class TracerouteApplication {
public:
    int Run(int argc, char* argv[]) {
        SetConsoleOutputCP(CP_UTF8);
        setlocale(LC_ALL, ".UTF-8");

        if (argc < 2) {
            TracerouteOptions opts;
            opts.PrintHelp(argv[0]);
            return 1;
        }

        TracerouteOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintHelp(argv[0]);
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintHelp(argv[0]);
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        if (opts.targetHostStr.empty()) {
            std::cerr << "Error: Target host specification missing.\n";
            return 1;
        }

        if (!NetworkUtils::IsUserAdmin()) {
            std::cerr << "[!] WARNING: Administrator privileges required to capture raw ICMP responses.\n"
                      << "    Please run terminal session as Administrator.\n\n";
        }

        WinsockScope winsock;
        if (!winsock.IsInitialized()) {
            std::cerr << "Error: WSAStartup failed.\n";
            return 1;
        }

        TracerouteEngine engine(opts);
        return engine.Execute() ? 0 : 1;
    }
};

int main(int argc, char* argv[]) {
    TracerouteApplication app;
    return app.Run(argc, argv);
}
