#pragma once

#include "vhdctl.hpp"

class ScopedVirtualDiskHandle {
public:
    explicit ScopedVirtualDiskHandle(HANDLE handle = nullptr);
    ~ScopedVirtualDiskHandle();

    ScopedVirtualDiskHandle(const ScopedVirtualDiskHandle&) = delete;
    ScopedVirtualDiskHandle& operator=(const ScopedVirtualDiskHandle&) = delete;

    ScopedVirtualDiskHandle(ScopedVirtualDiskHandle&& other) noexcept;
    ScopedVirtualDiskHandle& operator=(ScopedVirtualDiskHandle&& other) noexcept;

    HANDLE Get() const;
    HANDLE* AddressOf();
    bool IsValid() const;

    void Close();

private:
    HANDLE m_handle;
};
