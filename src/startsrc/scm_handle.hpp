#pragma once

#include "startsrc.hpp"

class ScmHandle {
private:
    SC_HANDLE m_handle{nullptr};

public:
    ScmHandle() = default;
    explicit ScmHandle(SC_HANDLE h);

    ~ScmHandle();

    ScmHandle(const ScmHandle&) = delete;
    ScmHandle& operator=(const ScmHandle&) = delete;

    ScmHandle(ScmHandle&& other) noexcept;

    ScmHandle& operator=(ScmHandle&& other) noexcept;

    void Close();

    SC_HANDLE Get() const;
    explicit operator bool() const;
};
