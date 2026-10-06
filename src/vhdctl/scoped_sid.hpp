#pragma once

#include "vhdctl.hpp"

class ScopedSid {
public:
    explicit ScopedSid(PSID sid = nullptr);
    ~ScopedSid();

    ScopedSid(const ScopedSid&) = delete;
    ScopedSid& operator=(const ScopedSid&) = delete;

    PSID Get() const;
    bool IsValid() const;

    void Close();

private:
    PSID m_sid;
};
