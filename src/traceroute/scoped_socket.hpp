#pragma once

#include "traceroute.hpp"

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
