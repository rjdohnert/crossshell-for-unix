#include "scoped_job_handle.hpp"

ScopedJobHandle::ScopedJobHandle(HANDLE handle ) : m_handle(handle) {}

ScopedJobHandle::~ScopedJobHandle() { Close(); }

ScopedJobHandle::ScopedJobHandle(ScopedJobHandle&& other) noexcept : m_handle(other.m_handle) { other.m_handle = NULL; }

ScopedJobHandle& ScopedJobHandle::operator=(ScopedJobHandle&& other) noexcept {
        if (this != &other) { Close(); m_handle = other.m_handle; other.m_handle = NULL; }
        return *this;
    }

HANDLE ScopedJobHandle::Get() const { return m_handle; }

bool ScopedJobHandle::IsValid() const { return m_handle != NULL && m_handle != INVALID_HANDLE_VALUE; }

void ScopedJobHandle::Close() {
        if (m_handle != NULL && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = NULL;
        }
    }
