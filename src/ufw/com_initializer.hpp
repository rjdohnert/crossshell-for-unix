#pragma once

#include "ufw.hpp"

class ComInitializer {
private:
    HRESULT m_hr;

public:
    ComInitializer();

    ~ComInitializer();

    bool Succeeded() const;
};
