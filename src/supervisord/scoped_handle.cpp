#include "scoped_handle.hpp"

ScopedHandle::ScopedHandle(HANDLE h ) : h_(h) {}

ScopedHandle::~ScopedHandle() { Close(); }

void ScopedHandle::Close() {
        if (h_ != INVALID_HANDLE_VALUE && h_ != NULL) {
            CloseHandle(h_);
            h_ = INVALID_HANDLE_VALUE;
        }
    }

HANDLE ScopedHandle::get() const { return h_; }

HANDLE* ScopedHandle::replace() { Close(); return &h_; }

bool ScopedHandle::isValid() const { return h_ != INVALID_HANDLE_VALUE && h_ != NULL; }

ScopedHandle::ScopedHandle(ScopedHandle&& o) noexcept : h_(o.h_) { o.h_ = INVALID_HANDLE_VALUE; }

ScopedHandle& ScopedHandle::operator=(ScopedHandle&& o) noexcept {
        if (this != &o) { Close(); h_ = o.h_; o.h_ = INVALID_HANDLE_VALUE; }
        return *this;
    }
