#pragma once

#include "threads.hpp"

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE);
    ~ScopedHandle();

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& other) noexcept;
    ScopedHandle& operator=(ScopedHandle&& other) noexcept;

    HANDLE get() const;
    bool isValid() const;

    void close();

private:
    HANDLE handle_;
};
