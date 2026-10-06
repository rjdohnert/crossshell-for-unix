#pragma once

#include "registryctl.hpp"

class ScopedHKey {
public:
    HKEY handle = nullptr;
    ScopedHKey() = default;
    explicit ScopedHKey(HKEY h);
    ~ScopedHKey();
    ScopedHKey(const ScopedHKey&) = delete;
    ScopedHKey& operator=(const ScopedHKey&) = delete;
    ScopedHKey(ScopedHKey&& other) noexcept;
    ScopedHKey& operator=(ScopedHKey&& other) noexcept;
    operator HKEY() const;
};
