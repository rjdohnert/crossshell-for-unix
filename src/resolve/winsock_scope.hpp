#pragma once

#include "resolve.hpp"

class WinsockScope {
public:
    WinsockScope();

    ~WinsockScope();

    bool IsInitialized() const;

private:
    bool m_initialized;
};
