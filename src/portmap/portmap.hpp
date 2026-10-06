#ifndef PORTMAP_HPP
#define PORTMAP_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <psapi.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <algorithm>
#include <unordered_map>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")

enum class ProtocolFilter {
    ALL,
    TCP_ONLY,
    UDP_ONLY
};

enum class IpFilter {
    ALL,
    IPV4_ONLY,
    IPV6_ONLY
};

struct ConnectionEntry {
    std::string proto;
    std::string localAddr;
    std::string foreignAddr;
    std::string state;
    DWORD pid = 0;
    std::string processName;
};

class WinsockScope {
public:
    WinsockScope();
    ~WinsockScope();
    bool IsInitialized() const { return m_initialized; }

private:
    bool m_initialized;
};

class ScopedProcessHandle {
public:
    explicit ScopedProcessHandle(HANDLE handle = nullptr);
    ~ScopedProcessHandle();

    ScopedProcessHandle(const ScopedProcessHandle&) = delete;
    ScopedProcessHandle& operator=(const ScopedProcessHandle&) = delete;

    ScopedProcessHandle(ScopedProcessHandle&& other) noexcept;
    ScopedProcessHandle& operator=(ScopedProcessHandle&& other) noexcept;

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE; }
    void Close();

private:
    HANDLE m_handle;
};

#endif // PORTMAP_HPP
