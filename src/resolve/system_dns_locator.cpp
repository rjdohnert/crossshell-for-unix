#include "system_dns_locator.hpp"

std::string SystemDnsLocator::GetPrimaryDnsServer() {
        ULONG outBufLen = sizeof(FIXED_INFO);
        std::vector<BYTE> buffer(outBufLen);
        FIXED_INFO* pFixedInfo = reinterpret_cast<FIXED_INFO*>(buffer.data());

        if (GetNetworkParams(pFixedInfo, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
            buffer.resize(outBufLen);
            pFixedInfo = reinterpret_cast<FIXED_INFO*>(buffer.data());
        }

        std::string dnsIp = "8.8.8.8";
        if (GetNetworkParams(pFixedInfo, &outBufLen) == NO_ERROR) {
            std::string candidate = pFixedInfo->DnsServerList.IpAddress.String;
            if (!candidate.empty() && candidate != "0.0.0.0") {
                dnsIp = candidate;
            }
        }

        return dnsIp;
    }
