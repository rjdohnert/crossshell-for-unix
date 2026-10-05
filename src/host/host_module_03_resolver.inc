// ============================================================================
// 3. HOST RESOLVER ENGINE
// ============================================================================

class HostResolver {
public:
    static bool IsIPv4(const std::wstring& value) {
        IN_ADDR a = {};
        return InetPtonW(AF_INET, value.c_str(), &a) == 1;
    }

    static bool IsIPv6(const std::wstring& value) {
        IN6_ADDR a = {};
        return InetPtonW(AF_INET6, value.c_str(), &a) == 1;
    }

    static std::wstring ToPtrName(const std::wstring& input) {
        if (IsIPv4(input)) {
            IN_ADDR a = {};
            InetPtonW(AF_INET, input.c_str(), &a);
            const unsigned char* b = reinterpret_cast<const unsigned char*>(&a.S_un.S_addr);
            wchar_t buf[128] = {};
            swprintf_s(buf, L"%u.%u.%u.%u.in-addr.arpa", b[3], b[2], b[1], b[0]);
            return buf;
        }

        if (IsIPv6(input)) {
            IN6_ADDR a6 = {};
            InetPtonW(AF_INET6, input.c_str(), &a6);
            std::wstring out;
            out.reserve(80);
            for (int i = 15; i >= 0; --i) {
                unsigned char byte = a6.u.Byte[i];
                unsigned char low = byte & 0x0F;
                unsigned char high = (byte >> 4) & 0x0F;
                wchar_t lb = (low < 10) ? (L'0' + low) : (L'a' + (low - 10));
                wchar_t hb = (high < 10) ? (L'0' + high) : (L'a' + (high - 10));
                out.push_back(lb);
                out.push_back(L'.');
                out.push_back(hb);
                out.push_back(L'.');
            }
            out += L"ip6.arpa";
            return out;
        }

        return input;
    }

    static std::wstring IPv4FromRecord(const DNS_A_DATA& data) {
        IN_ADDR a = {};
        a.S_un.S_addr = data.IpAddress;
        wchar_t buf[64] = {};
        if (!InetNtopW(AF_INET, &a, buf, 64)) return L"0.0.0.0";
        return buf;
    }

    static std::wstring IPv6FromRecord(const DNS_AAAA_DATA& data) {
        IN6_ADDR a6 = {};
        memcpy(&a6, data.Ip6Address.IP6Byte, 16);
        wchar_t buf[128] = {};
        if (!InetNtopW(AF_INET6, &a6, buf, 128)) return L"::";
        return buf;
    }

    bool Resolve(const std::wstring& target, std::vector<ResolvedRecord>& outRecords) {
        if (IsIPv4(target) || IsIPv6(target)) {
            ScopedDnsRecordList ptrRec;
            DNS_STATUS st = DnsQuery_W(ToPtrName(target).c_str(), DNS_TYPE_PTR, DNS_QUERY_STANDARD, nullptr, ptrRec.ReceiveHandle(), nullptr);
            if (st == ERROR_SUCCESS) {
                for (PDNS_RECORD cur = ptrRec.Get(); cur != nullptr; cur = cur->pNext) {
                    if (cur->wType == DNS_TYPE_PTR && cur->Data.PTR.pNameHost) {
                        ResolvedRecord rec;
                        rec.query = target;
                        rec.type = L"PTR";
                        rec.value = cur->Data.PTR.pNameHost;
                        rec.formattedLine = target + L" domain name pointer " + cur->Data.PTR.pNameHost;
                        outRecords.push_back(rec);
                    }
                }
            }
        } else {
            ScopedDnsRecordList recA;
            if (DnsQuery_W(target.c_str(), DNS_TYPE_A, DNS_QUERY_STANDARD, nullptr, recA.ReceiveHandle(), nullptr) == ERROR_SUCCESS) {
                for (PDNS_RECORD cur = recA.Get(); cur != nullptr; cur = cur->pNext) {
                    if (cur->wType == DNS_TYPE_A) {
                        ResolvedRecord rec;
                        rec.query = target;
                        rec.type = L"A";
                        rec.value = IPv4FromRecord(cur->Data.A);
                        rec.formattedLine = target + L" has address " + rec.value;
                        outRecords.push_back(rec);
                    }
                }
            }

            ScopedDnsRecordList recAAAA;
            if (DnsQuery_W(target.c_str(), DNS_TYPE_AAAA, DNS_QUERY_STANDARD, nullptr, recAAAA.ReceiveHandle(), nullptr) == ERROR_SUCCESS) {
                for (PDNS_RECORD cur = recAAAA.Get(); cur != nullptr; cur = cur->pNext) {
                    if (cur->wType == DNS_TYPE_AAAA) {
                        ResolvedRecord rec;
                        rec.query = target;
                        rec.type = L"AAAA";
                        rec.value = IPv6FromRecord(cur->Data.AAAA);
                        rec.formattedLine = target + L" has IPv6 address " + rec.value;
                        outRecords.push_back(rec);
                    }
                }
            }
        }

        return !outRecords.empty();
    }
};

