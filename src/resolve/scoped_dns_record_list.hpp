#pragma once

#include "resolve.hpp"

class ScopedDnsRecordList {
public:
    explicit ScopedDnsRecordList(PDNS_RECORD record = nullptr);

    ~ScopedDnsRecordList();

    ScopedDnsRecordList(const ScopedDnsRecordList&) = delete;
    ScopedDnsRecordList& operator=(const ScopedDnsRecordList&) = delete;

    ScopedDnsRecordList(ScopedDnsRecordList&& other) noexcept;

    ScopedDnsRecordList& operator=(ScopedDnsRecordList&& other) noexcept;

    PDNS_RECORD* ReceiveHandle();

    PDNS_RECORD Get() const;
    operator PDNS_RECORD() const;

private:
    void Free();

    PDNS_RECORD m_record;
};
