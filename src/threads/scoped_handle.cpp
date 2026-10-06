#include "scoped_handle.hpp"

ScopedHandle::ScopedHandle(HANDLE h ) : handle_(h) {}

ScopedHandle::~ScopedHandle() { close(); }

ScopedHandle::ScopedHandle(ScopedHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = INVALID_HANDLE_VALUE;
    }

ScopedHandle& ScopedHandle::operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            close();
            handle_ = other.handle_;
            other.handle_ = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

HANDLE ScopedHandle::get() const { return handle_; }

bool ScopedHandle::isValid() const { return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr; }

void ScopedHandle::close() {
        if (isValid()) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }
