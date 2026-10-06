#include "scoped_handle.hpp"

ScopedHandle::ScopedHandle(HANDLE h ) noexcept : m_handle(h) {}

ScopedHandle::~ScopedHandle() noexcept { reset(); }

ScopedHandle::ScopedHandle(ScopedHandle&& other) noexcept : m_handle(other.release()) {}

ScopedHandle& ScopedHandle::operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

[[nodiscard]] HANDLE ScopedHandle::get() const noexcept { return m_handle; }

[[nodiscard]] bool ScopedHandle::isValid() const noexcept { 
        return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr; 
    }

HANDLE ScopedHandle::release() noexcept {
        HANDLE tmp = m_handle;
        m_handle = INVALID_HANDLE_VALUE;
        return tmp;
    }

void ScopedHandle::reset(HANDLE h ) noexcept {
        if (isValid()) {
            ::CloseHandle(m_handle);
        }
        m_handle = h;
    }
