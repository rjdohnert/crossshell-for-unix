#include "scoped_hkey.hpp"

ScopedHKey::ScopedHKey(HKEY hKey ) : m_hKey(hKey) {}

ScopedHKey::~ScopedHKey() {
        if (m_hKey) RegCloseKey(m_hKey);
    }

HKEY ScopedHKey::get() const { return m_hKey; }

HKEY* ScopedHKey::receive() { return &m_hKey; }

bool ScopedHKey::isValid() const { return m_hKey != nullptr; }

ScopedHKey::ScopedHKey(ScopedHKey&& other) noexcept : m_hKey(other.m_hKey) {
        other.m_hKey = nullptr;
    }

ScopedHKey& ScopedHKey::operator=(ScopedHKey&& other) noexcept {
        if (this != &other) {
            if (m_hKey) RegCloseKey(m_hKey);
            m_hKey = other.m_hKey;
            other.m_hKey = nullptr;
        }
        return *this;
    }
