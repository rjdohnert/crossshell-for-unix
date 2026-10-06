#ifndef PBPASTE_HPP
#define PBPASTE_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <fcntl.h>
#include <io.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

class ScopedClipboard {
public:
    explicit ScopedClipboard(HWND owner = nullptr, int retries = 5, DWORD delayMs = 10) 
        : isOpen_(false) {
        for (int i = 0; i < retries; ++i) {
            if (OpenClipboard(owner)) {
                isOpen_ = true;
                break;
            }
            Sleep(delayMs);
        }
    }

    ~ScopedClipboard() {
        if (isOpen_) {
            CloseClipboard();
        }
    }

    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    ScopedClipboard(const ScopedClipboard&) = delete;
    ScopedClipboard& operator=(const ScopedClipboard&) = delete;

private:
    bool isOpen_;
};

template <typename T>
class ScopedGlobalLock {
public:
    explicit ScopedGlobalLock(HGLOBAL handle)
        : handle_(handle), ptr_(handle ? static_cast<T*>(GlobalLock(handle)) : nullptr) {}

    ~ScopedGlobalLock() {
        if (ptr_) {
            GlobalUnlock(handle_);
        }
    }

    [[nodiscard]] T* get() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    ScopedGlobalLock(const ScopedGlobalLock&) = delete;
    ScopedGlobalLock& operator=(const ScopedGlobalLock&) = delete;

private:
    HGLOBAL handle_;
    T* ptr_;
};

#endif // PBPASTE_HPP
