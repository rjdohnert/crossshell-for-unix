#include "scoped_dns_record_list.hpp"

ScopedDnsRecordList::ScopedDnsRecordList(PDNS_RECORD record ) : m_record(record) {}

ScopedDnsRecordList::~ScopedDnsRecordList() {
        Free();
    }

ScopedDnsRecordList::ScopedDnsRecordList(ScopedDnsRecordList&& other) noexcept : m_record(other.m_record) {
        other.m_record = nullptr;
    }

ScopedDnsRecordList& ScopedDnsRecordList::operator=(ScopedDnsRecordList&& other) noexcept {
        if (this != &other) {
            Free();
            m_record = other.m_record;
            other.m_record = nullptr;
        }
        return *this;
    }

PDNS_RECORD* ScopedDnsRecordList::ReceiveHandle() {
        Free();
        return &m_record;
    }

PDNS_RECORD ScopedDnsRecordList::Get() const { return m_record; }

ScopedDnsRecordList::operator PDNS_RECORD() const { return m_record; }

void ScopedDnsRecordList::Free() {
        if (m_record) {
            DnsRecordListFree(m_record, DnsFreeRecordList);
            m_record = nullptr;
        }
    }
