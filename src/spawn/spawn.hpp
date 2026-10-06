#ifndef SPAWN_HPP
#define SPAWN_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <optional>
#include <algorithm>
#include <cstdio>
#include <memory>

struct AutoHandle {
    HANDLE handle{ INVALID_HANDLE_VALUE };

    AutoHandle() = default;
    explicit AutoHandle(HANDLE h) : handle(h) {}
    ~AutoHandle() { if (isValid()) ::CloseHandle(handle); }

    AutoHandle(const AutoHandle&) = delete;
    AutoHandle& operator=(const AutoHandle&) = delete;

    AutoHandle(AutoHandle&& other) noexcept : handle(other.handle) {
        other.handle = INVALID_HANDLE_VALUE;
    }

    AutoHandle& operator=(AutoHandle&& other) noexcept {
        if (this != &other) {
            if (isValid()) ::CloseHandle(handle);
            handle = other.handle;
            other.handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    [[nodiscard]] bool isValid() const noexcept {
        return handle != INVALID_HANDLE_VALUE && handle != nullptr;
    }

    HANDLE get() const noexcept { return handle; }
    HANDLE* addressof() noexcept { return &handle; }
};

#endif // SPAWN_HPP
