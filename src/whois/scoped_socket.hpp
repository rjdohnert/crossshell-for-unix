#pragma once

#include "whois.hpp"

class ScopedSocket {
public:
    explicit ScopedSocket(SOCKET sock = INVALID_SOCKET);

    ~ScopedSocket();

    ScopedSocket(const ScopedSocket&) = delete;
    ScopedSocket& operator=(const ScopedSocket&) = delete;

    ScopedSocket(ScopedSocket&& other) noexcept;

    ScopedSocket& operator=(ScopedSocket&& other) noexcept;

    bool IsValid() const;
    SOCKET Get() const;
    operator SOCKET() const;

    void Close();

    void Reset(SOCKET sock = INVALID_SOCKET);

private:
    SOCKET m_socket;
};
