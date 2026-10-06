#pragma once

#include "stop.hpp"

class ScopedProcessHandle {
public:
    explicit ScopedProcessHandle(HANDLE handle = NULL);

    ~ScopedProcessHandle();

    ScopedProcessHandle(const ScopedProcessHandle&) = delete;
    ScopedProcessHandle& operator=(const ScopedProcessHandle&) = delete;

    ScopedProcessHandle(ScopedProcessHandle&& other) noexcept;

    ScopedProcessHandle& operator=(ScopedProcessHandle&& other) noexcept;

    HANDLE Get() const;
    bool IsValid() const;

    void Close();

private:
    HANDLE m_handle;
};
