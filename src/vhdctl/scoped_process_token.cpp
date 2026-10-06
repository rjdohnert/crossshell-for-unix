#include "scoped_process_token.hpp"

ScopedProcessToken::ScopedProcessToken(HANDLE token ) : m_token(token) {}

ScopedProcessToken::~ScopedProcessToken() { Close(); }

HANDLE ScopedProcessToken::Get() const { return m_token; }

bool ScopedProcessToken::IsValid() const { return m_token != nullptr && m_token != INVALID_HANDLE_VALUE; }

void ScopedProcessToken::Close() {
        if (IsValid()) {
            CloseHandle(m_token);
            m_token = nullptr;
        }
    }
