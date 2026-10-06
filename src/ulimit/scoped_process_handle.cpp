#include "scoped_process_handle.hpp"

ScopedProcessHandle::ScopedProcessHandle(HANDLE handle ) : m_handle(handle) {}

ScopedProcessHandle::~ScopedProcessHandle() { Close(); }

ScopedProcessHandle::ScopedProcessHandle(ScopedProcessHandle&& other) noexcept : m_handle(other.m_handle) { other.m_handle = NULL; }

ScopedProcessHandle& ScopedProcessHandle::operator=(ScopedProcessHandle&& other) noexcept {
        if (this != &other) { Close(); m_handle = other.m_handle; other.m_handle = NULL; }
        return *this;
    }

HANDLE ScopedProcessHandle::Get() const { return m_handle; }

bool ScopedProcessHandle::IsValid() const { return m_handle != NULL && m_handle != INVALID_HANDLE_VALUE; }

void ScopedProcessHandle::Close() {
        if (m_handle != NULL && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = NULL;
        }
    }
