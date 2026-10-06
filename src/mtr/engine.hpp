#ifndef MTR_ENGINE_HPP
#define MTR_ENGINE_HPP

#include "mtr.hpp"

class DnsService {
    std::unordered_map<std::string, std::string> cache_;
public:
    bool resolveHost(const std::string& host, IN_ADDR& addr);
    std::string reverseLookup(const std::string& ipStr);
};

class IcmpEngine {
    HANDLE hIcmp_{ INVALID_HANDLE_VALUE };
    std::vector<char> sendBuffer_;
    std::vector<uint8_t> replyBuffer_;

public:
    explicit IcmpEngine(size_t packetSize);
    ~IcmpEngine();

    struct ProbeOutcome {
        bool success{ false };
        bool isDestination{ false };
        std::string ip;
        double rtt{ 0.0 };
    };

    ProbeOutcome probe(IN_ADDR target, uint8_t ttl, DWORD timeoutMs);
};

#endif // MTR_ENGINE_HPP
