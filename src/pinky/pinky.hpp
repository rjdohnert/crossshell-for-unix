#ifndef PINKY_HPP
#define PINKY_HPP

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <lm.h>
#include <wtsapi32.h>

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <memory>

#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "Netapi32.lib")
#pragma comment(lib, "Ws2_32.lib")

namespace fs = std::filesystem;

class WinsockScope {
public:
    WinsockScope();
    ~WinsockScope();
    bool IsInitialized() const { return m_initialized; }

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

    bool IsValid() const { return m_socket != INVALID_SOCKET; }
    SOCKET Get() const { return m_socket; }
    operator SOCKET() const { return m_socket; }

    void Close();
    void Reset(SOCKET sock = INVALID_SOCKET);

private:
    SOCKET m_socket;
};

template <typename T>
class ScopedWtsMemory {
public:
    explicit ScopedWtsMemory(T* ptr = nullptr) : m_ptr(ptr) {}
    ~ScopedWtsMemory() { Free(); }

    ScopedWtsMemory(const ScopedWtsMemory&) = delete;
    ScopedWtsMemory& operator=(const ScopedWtsMemory&) = delete;

    ScopedWtsMemory(ScopedWtsMemory&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    ScopedWtsMemory& operator=(ScopedWtsMemory&& other) noexcept {
        if (this != &other) {
            Free();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    T* Get() const { return m_ptr; }
    operator T*() const { return m_ptr; }
    T* operator->() const { return m_ptr; }
    bool IsValid() const { return m_ptr != nullptr; }

    void Free() {
        if (m_ptr) {
            WTSFreeMemory(m_ptr);
            m_ptr = nullptr;
        }
    }

private:
    T* m_ptr;
};

template <typename T>
class ScopedNetApiMemory {
public:
    explicit ScopedNetApiMemory(T* ptr = nullptr) : m_ptr(ptr) {}
    ~ScopedNetApiMemory() { Free(); }

    ScopedNetApiMemory(const ScopedNetApiMemory&) = delete;
    ScopedNetApiMemory& operator=(const ScopedNetApiMemory&) = delete;

    ScopedNetApiMemory(ScopedNetApiMemory&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    ScopedNetApiMemory& operator=(ScopedNetApiMemory&& other) noexcept {
        if (this != &other) {
            Free();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    T* Get() const { return m_ptr; }
    operator T*() const { return m_ptr; }
    T* operator->() const { return m_ptr; }
    bool IsValid() const { return m_ptr != nullptr; }

    void Free() {
        if (m_ptr) {
            NetApiBufferFree(m_ptr);
            m_ptr = nullptr;
        }
    }

private:
    T* m_ptr;
};

struct PinkySessionSummary {
    std::wstring username;
    std::wstring fullName;
    std::wstring line;
    std::wstring idle;
    std::wstring logonTime;
    std::wstring host;
};

struct PinkyUserProfile {
    std::wstring username;
    std::wstring fullName;
    std::wstring homeDir;
    std::wstring comment;
    std::wstring shell;
    DWORD lastLogon = 0;
    bool userFound = false;
    std::vector<std::wstring> planLines;
};

#endif // PINKY_HPP
