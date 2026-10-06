#include "scoped_socket.hpp"

ScopedSocket::ScopedSocket(SOCKET sock ) : m_socket(sock) {}

ScopedSocket::~ScopedSocket() {
        Close();
    }

ScopedSocket::ScopedSocket(ScopedSocket&& other) noexcept : m_socket(other.m_socket) {
        other.m_socket = INVALID_SOCKET;
    }

ScopedSocket& ScopedSocket::operator=(ScopedSocket&& other) noexcept {
        if (this != &other) {
            Close();
            m_socket = other.m_socket;
            other.m_socket = INVALID_SOCKET;
        }
        return *this;
    }

bool ScopedSocket::IsValid() const { return m_socket != INVALID_SOCKET; }

SOCKET ScopedSocket::Get() const { return m_socket; }

ScopedSocket::operator SOCKET() const { return m_socket; }

void ScopedSocket::Close() {
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
    }

void ScopedSocket::Reset(SOCKET sock ) {
        Close();
        m_socket = sock;
    }
