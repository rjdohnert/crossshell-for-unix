#pragma once

#include "supervisord.hpp"

class ScopedHandle {
    HANDLE h_ = INVALID_HANDLE_VALUE;
public:
    ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE);
    ~ScopedHandle();
    void Close();
    HANDLE get() const;
    HANDLE* replace();
    bool isValid() const;
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& o) noexcept;
    ScopedHandle& operator=(ScopedHandle&& o) noexcept;
};
