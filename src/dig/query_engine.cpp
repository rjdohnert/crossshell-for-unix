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

#include "query_engine.hpp"
#include <cstring>

namespace dig {

std::wstring DnsQueryEngine::FormatIPv4(const DNS_A_DATA& data) {
    IN_ADDR a = {};
    a.S_un.S_addr = data.IpAddress;
    wchar_t buf[64] = {};
    if (!InetNtopW(AF_INET, &a, buf, 64)) return L"0.0.0.0";
    return buf;
}

std::wstring DnsQueryEngine::FormatIPv6(const DNS_AAAA_DATA& data) {
    IN6_ADDR a6 = {};
    memcpy(&a6, data.Ip6Address.IP6Byte, 16);
    wchar_t buf[128] = {};
    if (!InetNtopW(AF_INET6, &a6, buf, 128)) return L"::";
    return buf;
}

bool DnsQueryEngine::Query(const DigOptions& options, std::vector<DnsAnswerItem>& outAnswers, DWORD& outStatus) {
    ScopedDnsRecordList recordList;
    DNS_STATUS st = DnsQuery_W(options.name.c_str(), options.queryType, DNS_QUERY_STANDARD, nullptr, recordList.ReceiveHandle(), nullptr);
    outStatus = static_cast<DWORD>(st);
    if (st != ERROR_SUCCESS) {
        return false;
    }

    for (PDNS_RECORD cur = recordList.Get(); cur != nullptr; cur = cur->pNext) {
        DnsAnswerItem item;
        item.recordType = cur->wType;
        item.ttl = cur->dwTtl;

        switch (cur->wType) {
            case DNS_TYPE_A:
                item.typeName = L"A";
                item.data = FormatIPv4(cur->Data.A);
                break;
            case DNS_TYPE_AAAA:
                item.typeName = L"AAAA";
                item.data = FormatIPv6(cur->Data.AAAA);
                break;
            case DNS_TYPE_CNAME:
                item.typeName = L"CNAME";
                item.data = cur->Data.CNAME.pNameHost ? cur->Data.CNAME.pNameHost : L"";
                break;
            case DNS_TYPE_PTR:
                item.typeName = L"PTR";
                item.data = cur->Data.PTR.pNameHost ? cur->Data.PTR.pNameHost : L"";
                break;
            case DNS_TYPE_NS:
                item.typeName = L"NS";
                item.data = cur->Data.NS.pNameHost ? cur->Data.NS.pNameHost : L"";
                break;
            case DNS_TYPE_MX:
                item.typeName = L"MX";
                item.data = std::to_wstring(cur->Data.MX.wPreference) + L" " +
                            (cur->Data.MX.pNameExchange ? cur->Data.MX.pNameExchange : L"");
                break;
            case DNS_TYPE_TEXT:
                item.typeName = L"TXT";
                if (cur->Data.TXT.dwStringCount > 0 && cur->Data.TXT.pStringArray[0]) {
                    item.data = cur->Data.TXT.pStringArray[0];
                }
                break;
            default:
                item.typeName = options.queryTypeName;
                break;
        }

        if (!item.data.empty()) {
            outAnswers.push_back(item);
        }
    }

    return true;
}

} // namespace dig
