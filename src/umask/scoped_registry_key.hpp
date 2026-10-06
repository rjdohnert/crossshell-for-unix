#pragma once

#include "umask.hpp"

class ScopedRegistryKey {
public:
    explicit ScopedRegistryKey(HKEY key = NULL);

    ~ScopedRegistryKey();

    ScopedRegistryKey(const ScopedRegistryKey&) = delete;
    ScopedRegistryKey& operator=(const ScopedRegistryKey&) = delete;

    ScopedRegistryKey(ScopedRegistryKey&& other) noexcept;

    ScopedRegistryKey& operator=(ScopedRegistryKey&& other) noexcept;

    HKEY Get() const;
    HKEY* Receive();
    bool IsValid() const;

    void Close();

private:
    HKEY m_key;
};
