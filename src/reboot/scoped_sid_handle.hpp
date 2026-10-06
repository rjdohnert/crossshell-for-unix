#pragma once

#include "reboot.hpp"

class ScopedSidHandle {
public:
    explicit ScopedSidHandle(PSID sid = NULL);

    ~ScopedSidHandle();

    ScopedSidHandle(const ScopedSidHandle&) = delete;
    ScopedSidHandle& operator=(const ScopedSidHandle&) = delete;

    ScopedSidHandle(ScopedSidHandle&& other) noexcept;

    ScopedSidHandle& operator=(ScopedSidHandle&& other) noexcept;

    PSID Get() const;
    PSID* Receive();
    bool IsValid() const;

    void Free();

private:
    PSID m_sid;
};
