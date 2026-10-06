#pragma once

#include "top.hpp"

struct UniqueHandle {
    HANDLE handle = INVALID_HANDLE_VALUE;
    UniqueHandle(HANDLE h = INVALID_HANDLE_VALUE);
    ~UniqueHandle();
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& o) noexcept;
    UniqueHandle& operator=(UniqueHandle&& o) noexcept;
    operator HANDLE() const;
    bool isValid() const;
};
