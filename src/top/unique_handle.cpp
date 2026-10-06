#include "unique_handle.hpp"

UniqueHandle::UniqueHandle(HANDLE h ) : handle(h) {}

UniqueHandle::~UniqueHandle() { if (handle && handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }

UniqueHandle::UniqueHandle(UniqueHandle&& o) noexcept : handle(o.handle) { o.handle = INVALID_HANDLE_VALUE; }

UniqueHandle& UniqueHandle::operator=(UniqueHandle&& o) noexcept {
        if (this != &o) {
            if (handle && handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
            handle = o.handle;
            o.handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

UniqueHandle::operator HANDLE() const { return handle; }

bool UniqueHandle::isValid() const { return handle != nullptr && handle != INVALID_HANDLE_VALUE; }
