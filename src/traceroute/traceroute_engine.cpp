#include "network_utils.hpp"
#include "packet_headers.hpp"
#include "probe_result.hpp"
#include "scoped_socket.hpp"
#include "traceroute_engine.hpp"
#include "traceroute_options.hpp"

TracerouteEngine::TracerouteEngine(const TracerouteOptions& opts) : m_opts(opts) {}

bool TracerouteEngine::Execute() {
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
