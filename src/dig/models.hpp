/*
 * Copyright (c) 2025, R. J. Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DIG_MODELS_HPP
#define DIG_MODELS_HPP

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windns.h>

#include <string>

namespace dig {

class WinsockScope {
public:
    WinsockScope() : m_initialized(false) {
        WSADATA wsa = {};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
            m_initialized = true;
        }
    }

    ~WinsockScope() {
        if (m_initialized) {
            WSACleanup();
        }
    }

    bool IsInitialized() const { return m_initialized; }

private:
    bool m_initialized;
};

class ScopedDnsRecordList {
public:
    explicit ScopedDnsRecordList(PDNS_RECORD record = nullptr) : m_record(record) {}

    ~ScopedDnsRecordList() {
        Free();
    }

    ScopedDnsRecordList(const ScopedDnsRecordList&) = delete;
    ScopedDnsRecordList& operator=(const ScopedDnsRecordList&) = delete;

    ScopedDnsRecordList(ScopedDnsRecordList&& other) noexcept : m_record(other.m_record) {
        other.m_record = nullptr;
    }

    ScopedDnsRecordList& operator=(ScopedDnsRecordList&& other) noexcept {
        if (this != &other) {
            Free();
            m_record = other.m_record;
            other.m_record = nullptr;
        }
        return *this;
    }

    void Reset(PDNS_RECORD record = nullptr) {
        Free();
        m_record = record;
    }

    PDNS_RECORD* ReceiveHandle() {
        Free();
        return &m_record;
    }

    PDNS_RECORD Get() const { return m_record; }
    operator PDNS_RECORD() const { return m_record; }

private:
    void Free() {
        if (m_record) {
            DnsRecordListFree(m_record, DnsFreeRecordList);
            m_record = nullptr;
        }
    }

    PDNS_RECORD m_record;
};

struct DnsAnswerItem {
    WORD recordType = 0;
    std::wstring typeName;
    DWORD ttl = 0;
    std::wstring data;
};

enum class OutputFormat {
    Standard = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

} // namespace dig

#endif // DIG_MODELS_HPP
