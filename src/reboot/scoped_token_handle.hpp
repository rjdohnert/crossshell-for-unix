#pragma once

#include "reboot.hpp"

class ScopedTokenHandle {
public:
    explicit ScopedTokenHandle(HANDLE handle = NULL);

    ~ScopedTokenHandle();

    ScopedTokenHandle(const ScopedTokenHandle&) = delete;
    ScopedTokenHandle& operator=(const ScopedTokenHandle&) = delete;

    ScopedTokenHandle(ScopedTokenHandle&& other) noexcept;

    ScopedTokenHandle& operator=(ScopedTokenHandle&& other) noexcept;

    HANDLE Get() const;
    HANDLE* Receive();
    bool IsValid() const;

    void Close();

private:
    HANDLE m_handle;
};
