#include "resolve_engine.hpp"
#include "scoped_dns_record_list.hpp"
#include "string_encoding.hpp"
#include "system_dns_locator.hpp"

ResolveEngine::ResolveEngine() : m_queryTypeStr("A"), m_queryType(DNS_TYPE_A), m_debug(false) {
        m_dnsServer = SystemDnsLocator::GetPrimaryDnsServer();
        m_queryTypes = {
            {"A", DNS_TYPE_A},
            {"AAAA", DNS_TYPE_AAAA},
            {"MX", DNS_TYPE_MX},
            {"NS", DNS_TYPE_NS},
            {"PTR", DNS_TYPE_PTR},
            {"CNAME", DNS_TYPE_CNAME},
            {"SOA", DNS_TYPE_SOA},
            {"TXT", DNS_TYPE_TEXT},
            {"ANY", DNS_TYPE_ALL}
        };
    }

void ResolveEngine::SetServer(const std::string& serverInput) {
        in_addr addr4;
        if (inet_pton(AF_INET, serverInput.c_str(), &addr4) == 1) {
            m_dnsServer = serverInput;
            return;
        }

        addrinfo hints = {};
        hints.ai_family = AF_INET;
        addrinfo* res = nullptr;
        if (getaddrinfo(serverInput.c_str(), nullptr, &hints, &res) == 0 && res) {
            char ipBuf[INET_ADDRSTRLEN];
            sockaddr_in* s = reinterpret_cast<sockaddr_in*>(res->ai_addr);
            inet_ntop(AF_INET, &s->sin_addr, ipBuf, INET_ADDRSTRLEN);
            m_dnsServer = ipBuf;
            freeaddrinfo(res);
        } else {
            std::cerr << "*** Can't resolve server name '" << serverInput << "'\n";
        }
    }

std::string ResolveEngine::GetServer() const { return m_dnsServer; }

bool ResolveEngine::SetQueryType(const std::string& type) {
        std::string upperType = type;
        std::transform(upperType.begin(), upperType.end(), upperType.begin(), ::toupper);

        auto it = m_queryTypes.find(upperType);
        if (it != m_queryTypes.end()) {
            m_queryTypeStr = upperType;
            m_queryType = it->second;
            return true;
        }
        return false;
    }

std::string ResolveEngine::GetQueryType() const { return m_queryTypeStr; }

void ResolveEngine::SetDebug(bool debug) { m_debug = debug; }

bool ResolveEngine::GetDebug() const { return m_debug; }

bool ResolveEngine::ExecuteQuery(const std::string& targetInput) {
        std::string target = targetInput;

        in_addr addr4;
        in6_addr addr6;
        if (inet_pton(AF_INET, target.c_str(), &addr4) == 1) {
            if (m_queryType == DNS_TYPE_A) {
                m_queryTypeStr = "PTR";
                m_queryType = DNS_TYPE_PTR;
            }
            if (m_queryType == DNS_TYPE_PTR) {
                target = StringEncoding::FormatInAddrArpa(target);
            }
        } else if (inet_pton(AF_INET6, target.c_str(), &addr6) == 1) {
            if (m_queryType == DNS_TYPE_A || m_queryType == DNS_TYPE_AAAA) {
                m_queryTypeStr = "PTR";
                m_queryType = DNS_TYPE_PTR;
            }
            if (m_queryType == DNS_TYPE_PTR) {
                target = StringEncoding::FormatIp6Arpa(target);
            }
        }

        std::cout << "Server:  " << m_dnsServer << "\n";
        std::cout << "Address: " << m_dnsServer << "#53\n\n";

        IP4_ARRAY dnsServers = {};
        dnsServers.AddrCount = 1;
        inet_pton(AF_INET, m_dnsServer.c_str(), &dnsServers.AddrArray[0]);

        ScopedDnsRecordList recordList;
        std::wstring wTarget = StringEncoding::Utf8ToWide(target);

        if (m_debug) {
            std::cout << "------------\n"
                      << "Got answer:\n"
                      << "    HEADER:\n"
                      << "        opcode = QUERY, status = NOERROR, id = 1\n"
                      << "        flags: qr rd ra; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 0\n\n"
                      << "    QUESTIONS:\n"
                      << "        " << target << ", type = " << m_queryTypeStr << ", class = IN\n\n"
                      << "    ANSWERS:\n";
        }

        DNS_STATUS status = DnsQuery_W(
            wTarget.c_str(),
            m_queryType,
            DNS_QUERY_STANDARD,
            &dnsServers,
            recordList.ReceiveHandle(),
            nullptr
        );

        if (status != ERROR_SUCCESS) {
            if (status == DNS_INFO_NO_RECORDS || status == DNS_ERROR_RCODE_NAME_ERROR) {
                std::cout << "*** " << m_dnsServer << " can't find " << targetInput << ": Non-existent domain\n";
            } else {
                std::cout << "*** " << m_dnsServer << " can't find " << targetInput << ": Query failed (Error: " << status << ")\n";
            }
            return false;
        }

        std::cout << "Non-authoritative answer:\n";

        for (PDNS_RECORD pCurr = recordList.Get(); pCurr != nullptr; pCurr = pCurr->pNext) {
            std::string name = StringEncoding::WideToUtf8(pCurr->pName);

            switch (pCurr->wType) {
            case DNS_TYPE_A: {
                in_addr ip;
                ip.s_addr = pCurr->Data.A.IpAddress;
                char ipBuf[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &ip, ipBuf, INET_ADDRSTRLEN);
                std::cout << "Name:    " << name << "\n"
                          << "Address: " << ipBuf << "\n";
                break;
            }
            case DNS_TYPE_AAAA: {
                char ipBuf[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &pCurr->Data.AAAA.Ip6Address, ipBuf, INET6_ADDRSTRLEN);
                std::cout << "Name:    " << name << "\n"
                          << "Address: " << ipBuf << "\n";
                break;
            }
            case DNS_TYPE_MX: {
                std::string exchange = StringEncoding::WideToUtf8(pCurr->Data.MX.pNameExchange);
                std::cout << name << "\tmail exchanger = " << pCurr->Data.MX.wPreference << " " << exchange << "\n";
                break;
            }
            case DNS_TYPE_NS: {
                std::string ns = StringEncoding::WideToUtf8(pCurr->Data.NS.pNameHost);
                std::cout << name << "\tnameserver = " << ns << "\n";
                break;
            }
            case DNS_TYPE_PTR: {
                std::string ptr = StringEncoding::WideToUtf8(pCurr->Data.PTR.pNameHost);
                std::cout << targetInput << "\tname = " << ptr << "\n";
                break;
            }
            case DNS_TYPE_CNAME: {
                std::string cname = StringEncoding::WideToUtf8(pCurr->Data.CNAME.pNameHost);
                std::cout << name << "\tcanonical name = " << cname << "\n";
                break;
            }
            case DNS_TYPE_SOA: {
                std::string primary = StringEncoding::WideToUtf8(pCurr->Data.SOA.pNamePrimaryServer);
                std::string admin = StringEncoding::WideToUtf8(pCurr->Data.SOA.pNameAdministrator);
                std::cout << name << "\n"
                          << "\tprimary name server = " << primary << "\n"
                          << "\tresponsible mail addr = " << admin << "\n"
                          << "\tserial  = " << pCurr->Data.SOA.dwSerialNo << "\n"
                          << "\trefresh = " << pCurr->Data.SOA.dwRefresh << "\n"
                          << "\tretry   = " << pCurr->Data.SOA.dwRetry << "\n"
                          << "\texpire  = " << pCurr->Data.SOA.dwExpire << "\n"
                          << "\tdefault TTL = " << pCurr->Data.SOA.dwDefaultTtl << "\n";
                break;
            }
            case DNS_TYPE_TEXT: {
                std::cout << name << "\ttext = ";
                if (pCurr->Data.TXT.pStringArray) {
                    for (DWORD i = 0; i < pCurr->Data.TXT.dwStringCount; ++i) {
                        std::cout << "\"" << StringEncoding::WideToUtf8(pCurr->Data.TXT.pStringArray[i]) << "\" ";
                    }
                }
                std::cout << "\n";
                break;
            }
            default:
                break;
            }
        }

        if (m_debug) {
            std::cout << "------------\n";
        }

        return true;
    }
