#include "winsock_scope.hpp"

WinsockScope::WinsockScope() : m_initialized(false) {
        WSADATA wsaData = {};
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            m_initialized = true;
        }
    }

WinsockScope::~WinsockScope() {
        if (m_initialized) {
            WSACleanup();
        }
    }

bool WinsockScope::IsInitialized() const { return m_initialized; }
