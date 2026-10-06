#include "scoped_hkey.hpp"

ScopedHKey::ScopedHKey(HKEY h) : handle(h) {}

ScopedHKey::~ScopedHKey() { if (handle && handle != INVALID_HANDLE_VALUE) RegCloseKey(handle); }

ScopedHKey::ScopedHKey(ScopedHKey&& other) noexcept : handle(other.handle) { other.handle = nullptr; }

ScopedHKey& ScopedHKey::operator=(ScopedHKey&& other) noexcept {
        if (this != &other) {
            if (handle && handle != INVALID_HANDLE_VALUE) RegCloseKey(handle);
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }

ScopedHKey::operator HKEY() const { return handle; }
