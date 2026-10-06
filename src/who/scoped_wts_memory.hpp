#pragma once

#include "who.hpp"

template <typename T>
class ScopedWtsMemory {
public:
    explicit ScopedWtsMemory(T* ptr = nullptr) : m_ptr(ptr) {}

    ~ScopedWtsMemory() { Free(); }

    ScopedWtsMemory(const ScopedWtsMemory&) = delete;
    ScopedWtsMemory& operator=(const ScopedWtsMemory&) = delete;

    ScopedWtsMemory(ScopedWtsMemory&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    ScopedWtsMemory& operator=(ScopedWtsMemory&& other) noexcept {
        if (this != &other) {
            Free();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    T* Get() const { return m_ptr; }
    T** Receive() { Free(); return &m_ptr; }
    operator T*() const { return m_ptr; }
    T* operator->() const { return m_ptr; }
    bool IsValid() const { return m_ptr != nullptr; }

    void Free() {
        if (m_ptr) {
            WTSFreeMemory(m_ptr);
            m_ptr = nullptr;
        }
    }

private:
    T* m_ptr;
};
