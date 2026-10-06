#include "scoped_virtual_disk_handle.hpp"

ScopedVirtualDiskHandle::ScopedVirtualDiskHandle(HANDLE handle ) : m_handle(handle) {}

ScopedVirtualDiskHandle::~ScopedVirtualDiskHandle() { Close(); }

ScopedVirtualDiskHandle::ScopedVirtualDiskHandle(ScopedVirtualDiskHandle&& other) noexcept : m_handle(other.m_handle) { other.m_handle = nullptr; }

ScopedVirtualDiskHandle& ScopedVirtualDiskHandle::operator=(ScopedVirtualDiskHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

HANDLE ScopedVirtualDiskHandle::Get() const { return m_handle; }

HANDLE* ScopedVirtualDiskHandle::AddressOf() { return &m_handle; }

bool ScopedVirtualDiskHandle::IsValid() const { return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE; }

void ScopedVirtualDiskHandle::Close() {
        if (IsValid()) {
            CloseHandle(m_handle);
            m_handle = nullptr;
        }
    }
