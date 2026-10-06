#include "scoped_token_handle.hpp"

ScopedTokenHandle::ScopedTokenHandle(HANDLE handle ) : m_handle(handle) {}

ScopedTokenHandle::~ScopedTokenHandle() {
        Close();
    }

ScopedTokenHandle::ScopedTokenHandle(ScopedTokenHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = NULL;
    }

ScopedTokenHandle& ScopedTokenHandle::operator=(ScopedTokenHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = NULL;
        }
        return *this;
    }

HANDLE ScopedTokenHandle::Get() const { return m_handle; }

HANDLE* ScopedTokenHandle::Receive() { Close(); return &m_handle; }

bool ScopedTokenHandle::IsValid() const { return m_handle != NULL && m_handle != INVALID_HANDLE_VALUE; }

void ScopedTokenHandle::Close() {
        if (m_handle != NULL && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = NULL;
        }
    }
