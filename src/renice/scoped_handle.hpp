#pragma once

#include "renice.hpp"

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) noexcept;
    ~ScopedHandle() noexcept;

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& other) noexcept;
    ScopedHandle& operator=(ScopedHandle&& other) noexcept;

    [[nodiscard]] HANDLE get() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;

    HANDLE release() noexcept;

    void reset(HANDLE h = INVALID_HANDLE_VALUE) noexcept;

private:
    HANDLE m_handle;
};
