#ifndef MTR_HPP
#define MTR_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <conio.h>

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <chrono>
#include <thread>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <cmath>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

std::string sanitizeTerminalText(const std::string& text);

class WinsockGuard {
public:
    WinsockGuard();
    ~WinsockGuard();
};

class TerminalScreenGuard {
    HANDLE hOut_;
    DWORD originalMode_{ 0 };
public:
    TerminalScreenGuard();
    ~TerminalScreenGuard();
};

class HopRecord {
public:
    uint8_t ttl{ 0 };
    std::string ip{ "???" };
    std::string hostname{ "" };
    bool reachedDestination{ false };

    size_t sent{ 0 };
    size_t received{ 0 };
    double lastRtt{ 0.0 };
    double minRtt{ 999999.0 };
    double maxRtt{ 0.0 };
    double sumRtt{ 0.0 };
    double sumSqRtt{ 0.0 };

    explicit HopRecord(uint8_t hopTtl = 0);

    void addResult(const std::string& responderIp, double rttMs, bool isDestination);
    void addTimeout();
    void reset();

    [[nodiscard]] double lossPercent() const noexcept;
    [[nodiscard]] double avgRtt() const noexcept;
    [[nodiscard]] double bestRtt() const noexcept;
    [[nodiscard]] double stdDev() const noexcept;
};

#endif // MTR_HPP
