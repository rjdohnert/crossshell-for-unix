#include "scoped_transaction.hpp"

ScopedTransaction::ScopedTransaction() {
        m_hTx = CreateTransaction(nullptr, 0, 0, 0, 0, 0, const_cast<LPWSTR>(L"registryctl_atomic_tx"));
    }

ScopedTransaction::~ScopedTransaction() {
        if (m_hTx != INVALID_HANDLE_VALUE) {
            if (!m_committed) RollbackTransaction(m_hTx);
            CloseHandle(m_hTx);
        }
    }

bool ScopedTransaction::IsValid() const { return m_hTx != INVALID_HANDLE_VALUE; }

HANDLE ScopedTransaction::Get() const { return m_hTx; }

bool ScopedTransaction::Commit() {
        if (m_hTx == INVALID_HANDLE_VALUE || m_committed) return false;
        if (CommitTransaction(m_hTx)) {
            m_committed = true;
            return true;
        }
        return false;
    }

bool ScopedTransaction::Rollback() {
        if (m_hTx == INVALID_HANDLE_VALUE) return false;
        return RollbackTransaction(m_hTx) != FALSE;
    }
