#include "scoped_service_handle.hpp"

ScopedServiceHandle::ScopedServiceHandle(SC_HANDLE handle ) : m_handle(handle) {}

ScopedServiceHandle::~ScopedServiceHandle() {
        Close();
    }

ScopedServiceHandle::ScopedServiceHandle(ScopedServiceHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = NULL;
    }

ScopedServiceHandle& ScopedServiceHandle::operator=(ScopedServiceHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = NULL;
        }
        return *this;
    }

SC_HANDLE ScopedServiceHandle::Get() const { return m_handle; }

SC_HANDLE* ScopedServiceHandle::Receive() { Close(); return &m_handle; }

bool ScopedServiceHandle::IsValid() const { return m_handle != NULL; }

ScopedServiceHandle::operator SC_HANDLE() const { return m_handle; }

void ScopedServiceHandle::Close() {
        if (m_handle != NULL) {
            CloseServiceHandle(m_handle);
            m_handle = NULL;
        }
    }

void ScopedServiceHandle::Reset(SC_HANDLE handle ) {
        Close();
        m_handle = handle;
    }
