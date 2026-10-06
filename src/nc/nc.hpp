#ifndef NC_HPP
#define NC_HPP

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mstcpip.h>
#include <io.h>
#include <fcntl.h>

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <memory>

#pragma comment(lib, "ws2_32.lib")

class WinsockScope {
public:
    WinsockScope();
    ~WinsockScope();
    bool IsInitialized() const;
private:
    bool m_initialized;
};

class ScopedSocket {
public:
    explicit ScopedSocket(SOCKET sock = INVALID_SOCKET);
    ~ScopedSocket();
    ScopedSocket(const ScopedSocket&) = delete;
    ScopedSocket& operator=(const ScopedSocket&) = delete;
    ScopedSocket(ScopedSocket&& other) noexcept;
    ScopedSocket& operator=(ScopedSocket&& other) noexcept;

    SOCKET Get() const;
    bool IsValid() const;
    void Close();
    void Reset(SOCKET s = INVALID_SOCKET);

private:
    SOCKET m_socket;
};

#endif // NC_HPP
