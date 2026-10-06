#include "scoped_registry_key.hpp"

ScopedRegistryKey::ScopedRegistryKey(HKEY key ) : m_key(key) {}

ScopedRegistryKey::~ScopedRegistryKey() {
        Close();
    }

ScopedRegistryKey::ScopedRegistryKey(ScopedRegistryKey&& other) noexcept : m_key(other.m_key) {
        other.m_key = NULL;
    }

ScopedRegistryKey& ScopedRegistryKey::operator=(ScopedRegistryKey&& other) noexcept {
        if (this != &other) {
            Close();
            m_key = other.m_key;
            other.m_key = NULL;
        }
        return *this;
    }

HKEY ScopedRegistryKey::Get() const { return m_key; }

HKEY* ScopedRegistryKey::Receive() { Close(); return &m_key; }

bool ScopedRegistryKey::IsValid() const { return m_key != NULL; }

void ScopedRegistryKey::Close() {
        if (m_key != NULL) {
            RegCloseKey(m_key);
            m_key = NULL;
        }
    }
