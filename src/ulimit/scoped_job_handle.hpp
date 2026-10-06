#pragma once

#include "ulimit.hpp"

class ScopedJobHandle {
public:
    explicit ScopedJobHandle(HANDLE handle = NULL);
    ~ScopedJobHandle();
    ScopedJobHandle(const ScopedJobHandle&) = delete;
    ScopedJobHandle& operator=(const ScopedJobHandle&) = delete;
    ScopedJobHandle(ScopedJobHandle&& other) noexcept;
    ScopedJobHandle& operator=(ScopedJobHandle&& other) noexcept;
    HANDLE Get() const;
    bool IsValid() const;
    void Close();
private:
    HANDLE m_handle;
};
