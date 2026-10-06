#pragma once

#include "registryctl.hpp"

class ScopedTransaction {
private:
    HANDLE m_hTx = INVALID_HANDLE_VALUE;
    bool m_committed = false;
public:
    ScopedTransaction();
    ~ScopedTransaction();
    bool IsValid() const;
    HANDLE Get() const;
    bool Commit();
    bool Rollback();
};
