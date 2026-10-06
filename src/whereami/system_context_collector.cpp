#include "location_report.hpp"
#include "system_context_collector.hpp"

void SystemContextCollector::populate(LocationReport& report) {
        char host[256] = {0};
        if (gethostname(host, sizeof(host)) == 0) {
            report.hostname = host;
        }

        // Retrieve primary local IP address
        ULONG outBufLen = 15000;
        std::vector<BYTE> buf(outBufLen);
        PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());

        if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &outBufLen) == NO_ERROR) {
            for (PIP_ADAPTER_ADDRESSES pCurr = pAddresses; pCurr != NULL; pCurr = pCurr->Next) {
                if (pCurr->OperStatus == IfOperStatusUp && pCurr->IfType != IF_TYPE_SOFTWARE_LOOPBACK) {
                    PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurr->FirstUnicastAddress;
                    if (pUnicast) {
                        sockaddr_in* sa_in = reinterpret_cast<sockaddr_in*>(pUnicast->Address.lpSockaddr);
                        char ip[INET_ADDRSTRLEN] = {0};
                        inet_ntop(AF_INET, &(sa_in->sin_addr), ip, INET_ADDRSTRLEN);
                        report.local_ip = ip;
                        break;
                    }
                }
            }
        }
    }
