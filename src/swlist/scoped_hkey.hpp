#pragma once

#include "swlist.hpp"

class ScopedHKey {
    HKEY m_hKey;
public:
    explicit ScopedHKey(HKEY hKey = nullptr);
    ~ScopedHKey();

    HKEY get() const;
    HKEY* receive();
    bool isValid() const;

    // Prevent Copying
    ScopedHKey(const ScopedHKey&) = delete;
    ScopedHKey& operator=(const ScopedHKey&) = delete;

    // Allow Moving
    ScopedHKey(ScopedHKey&& other) noexcept;
    ScopedHKey& operator=(ScopedHKey&& other) noexcept;
};

// --- Data Structures ---
