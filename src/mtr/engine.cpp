#include "engine.hpp"

std::string sanitizeTerminalText(const std::string& text) {
    std::string sanitized;
    sanitized.reserve(text.size());
    for (unsigned char ch : text) {
        sanitized.push_back(ch >= 0x20 && ch != 0x7f ? static_cast<char>(ch) : '?');
    }
    return sanitized;
}

WinsockGuard::WinsockGuard() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        throw std::runtime_error("Unable to initialize Winsock 2.2.");
    }
}

WinsockGuard::~WinsockGuard() {
    WSACleanup();
}

TerminalScreenGuard::TerminalScreenGuard() : hOut_(GetStdHandle(STD_OUTPUT_HANDLE)) {
    GetConsoleMode(hOut_, &originalMode_);
    SetConsoleMode(hOut_, originalMode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    std::cout << "\033[?25l";
}

TerminalScreenGuard::~TerminalScreenGuard() {
    std::cout << "\033[?25h\033[0m";
    SetConsoleMode(hOut_, originalMode_);
}

HopRecord::HopRecord(uint8_t hopTtl) : ttl(hopTtl) {}

void HopRecord::addResult(const std::string& responderIp, double rttMs, bool isDestination) {
    sent++;
    received++;
    if (ip != responderIp) {
        hostname.clear();
    }
    ip = responderIp;
    lastRtt = rttMs;
    minRtt = (std::min)(minRtt, rttMs);
    maxRtt = (std::max)(maxRtt, rttMs);
    sumRtt += rttMs;
    sumSqRtt += (rttMs * rttMs);
    reachedDestination = isDestination;
}

void HopRecord::addTimeout() {
    sent++;
}

void HopRecord::reset() {
    sent = 0;
    received = 0;
    lastRtt = 0.0;
    minRtt = 999999.0;
    maxRtt = 0.0;
    sumRtt = 0.0;
    sumSqRtt = 0.0;
}

double HopRecord::lossPercent() const noexcept {
    if (sent == 0) return 0.0;
    return (1.0 - (static_cast<double>(received) / static_cast<double>(sent))) * 100.0;
}

double HopRecord::avgRtt() const noexcept {
    return (received > 0) ? (sumRtt / received) : 0.0;
}

double HopRecord::bestRtt() const noexcept {
    return (received > 0) ? minRtt : 0.0;
}

double HopRecord::stdDev() const noexcept {
    if (received <= 1) return 0.0;
    double avg = avgRtt();
    double variance = (sumSqRtt / received) - (avg * avg);
    return std::sqrt((std::max)(0.0, variance));
}

bool DnsService::resolveHost(const std::string& host, IN_ADDR& addr) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;

    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0 || !result) {
        return false;
    }

    addr = reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr;
    freeaddrinfo(result);
    return true;
}

std::string DnsService::reverseLookup(const std::string& ipStr) {
    auto it = cache_.find(ipStr);
    if (it != cache_.end()) {
        return it->second;
    }

    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    if (inet_pton(AF_INET, ipStr.c_str(), &(sa.sin_addr)) != 1) {
        cache_[ipStr] = "";
        return "";
    }

    char hostBuf[NI_MAXHOST];
    if (getnameinfo(reinterpret_cast<sockaddr*>(&sa), sizeof(sa),
                    hostBuf, sizeof(hostBuf), nullptr, 0, NI_NAMEREQD) == 0) {
        cache_[ipStr] = hostBuf;
        return hostBuf;
    }

    cache_[ipStr] = "";
    return "";
}

IcmpEngine::IcmpEngine(size_t packetSize) : sendBuffer_(packetSize, 'E') {
    hIcmp_ = IcmpCreateFile();
    if (hIcmp_ == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to obtain handle from IcmpCreateFile.");
    }
    size_t replySize = sizeof(ICMP_ECHO_REPLY) + packetSize + 64;
    replyBuffer_.resize(replySize);
}

IcmpEngine::~IcmpEngine() {
    if (hIcmp_ != INVALID_HANDLE_VALUE) {
        IcmpCloseHandle(hIcmp_);
    }
}

IcmpEngine::ProbeOutcome IcmpEngine::probe(IN_ADDR target, uint8_t ttl, DWORD timeoutMs) {
    IP_OPTION_INFORMATION options{};
    options.Ttl = ttl;

    auto start = std::chrono::high_resolution_clock::now();
    DWORD replies = IcmpSendEcho(
        hIcmp_,
        target.S_un.S_addr,
        sendBuffer_.data(),
        static_cast<WORD>(sendBuffer_.size()),
        &options,
        replyBuffer_.data(),
        static_cast<DWORD>(replyBuffer_.size()),
        timeoutMs
    );
    auto end = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();

    ProbeOutcome outcome;
    if (replies > 0) {
        auto* echoReply = reinterpret_cast<PICMP_ECHO_REPLY>(replyBuffer_.data());
        IN_ADDR responder{};
        responder.S_un.S_addr = echoReply->Address;

        char ipStr[INET_ADDRSTRLEN]{};
        if (!inet_ntop(AF_INET, &responder, ipStr, sizeof(ipStr))) {
            return outcome;
        }
        outcome.ip = ipStr;
        outcome.rtt = (echoReply->RoundTripTime > 0) 
                    ? static_cast<double>(echoReply->RoundTripTime) 
                    : elapsedMs;

        if (echoReply->Status == IP_SUCCESS) {
            outcome.success = true;
            outcome.isDestination = true;
        } else if (echoReply->Status == IP_TTL_EXPIRED_TRANSIT) {
            outcome.success = true;
            outcome.isDestination = false;
        }
    }
    return outcome;
}
