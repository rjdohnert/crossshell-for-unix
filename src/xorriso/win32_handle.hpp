#pragma once

#include "xorriso.hpp"

class Win32Handle {
    HANDLE h_ = INVALID_HANDLE_VALUE;
public:
    Win32Handle(HANDLE h);
    ~Win32Handle();
    bool isValid() const;
    operator HANDLE() const;
};
