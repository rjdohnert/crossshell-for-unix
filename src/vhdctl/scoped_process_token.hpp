#pragma once

#include "vhdctl.hpp"

class ScopedProcessToken {
public:
    explicit ScopedProcessToken(HANDLE token = nullptr);
    ~ScopedProcessToken();

    ScopedProcessToken(const ScopedProcessToken&) = delete;
    ScopedProcessToken& operator=(const ScopedProcessToken&) = delete;

    HANDLE Get() const;
    bool IsValid() const;

    void Close();

private:
    HANDLE m_token;
};
