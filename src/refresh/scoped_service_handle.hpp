#pragma once

#include "refresh.hpp"

class ScopedServiceHandle {
public:
    explicit ScopedServiceHandle(SC_HANDLE handle = NULL);

    ~ScopedServiceHandle();

    ScopedServiceHandle(const ScopedServiceHandle&) = delete;
    ScopedServiceHandle& operator=(const ScopedServiceHandle&) = delete;

    ScopedServiceHandle(ScopedServiceHandle&& other) noexcept;

    ScopedServiceHandle& operator=(ScopedServiceHandle&& other) noexcept;

    SC_HANDLE Get() const;
    SC_HANDLE* Receive();
    bool IsValid() const;
    operator SC_HANDLE() const;

    void Close();

    void Reset(SC_HANDLE handle = NULL);

private:
    SC_HANDLE m_handle;
};
