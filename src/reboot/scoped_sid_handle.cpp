#include "scoped_sid_handle.hpp"

ScopedSidHandle::ScopedSidHandle(PSID sid ) : m_sid(sid) {}

ScopedSidHandle::~ScopedSidHandle() {
        Free();
    }

ScopedSidHandle::ScopedSidHandle(ScopedSidHandle&& other) noexcept : m_sid(other.m_sid) {
        other.m_sid = NULL;
    }

ScopedSidHandle& ScopedSidHandle::operator=(ScopedSidHandle&& other) noexcept {
        if (this != &other) {
            Free();
            m_sid = other.m_sid;
            other.m_sid = NULL;
        }
        return *this;
    }

PSID ScopedSidHandle::Get() const { return m_sid; }

PSID* ScopedSidHandle::Receive() { Free(); return &m_sid; }

bool ScopedSidHandle::IsValid() const { return m_sid != NULL; }

void ScopedSidHandle::Free() {
        if (m_sid != NULL) {
            FreeSid(m_sid);
            m_sid = NULL;
        }
    }
