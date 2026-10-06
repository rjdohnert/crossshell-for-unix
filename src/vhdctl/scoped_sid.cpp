#include "scoped_sid.hpp"

ScopedSid::ScopedSid(PSID sid ) : m_sid(sid) {}

ScopedSid::~ScopedSid() { Close(); }

PSID ScopedSid::Get() const { return m_sid; }

bool ScopedSid::IsValid() const { return m_sid != nullptr; }

void ScopedSid::Close() {
        if (IsValid()) {
            FreeSid(m_sid);
            m_sid = nullptr;
        }
    }
