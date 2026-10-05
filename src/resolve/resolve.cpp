/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <windns.h>
#include <iphlpapi.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <algorithm>
#include <memory>
#include <clocale>

#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

// ============================================================================
// 1. RAII SCOPES & STRING CONVERTERS
// ============================================================================

class WinsockScope {
public:
    WinsockScope() : m_initialized(false) {
        WSADATA wsaData = {};
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
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

class StringEncoding {
public:
    static std::string WideToUtf8(const wchar_t* wstr) {
        if (!wstr) return "";
        int len = static_cast<int>(wcslen(wstr));
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr, len, nullptr, 0, nullptr, nullptr);
        if (size <= 0) return "";
        std::string str(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr, len, &str[0], size, nullptr, nullptr);
        return str;
    }

    static std::wstring Utf8ToWide(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        if (size <= 0) return L"";
        std::wstring wstr(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
        if (!wstr.empty() && wstr.back() == L'\0') wstr.pop_back();
        return wstr;
    }

    static std::string FormatInAddrArpa(const std::string& ipStr) {
        in_addr addr;
        if (inet_pton(AF_INET, ipStr.c_str(), &addr) == 1) {
            BYTE* bytes = reinterpret_cast<BYTE*>(&addr.s_addr);
            char buf[128];
            snprintf(buf, sizeof(buf), "%d.%d.%d.%d.in-addr.arpa",
                     bytes[3], bytes[2], bytes[1], bytes[0]);
            return std::string(buf);
        }
        return ipStr;
    }

    static std::string FormatIp6Arpa(const std::string& ipStr) {
        in6_addr addr;
        if (inet_pton(AF_INET6, ipStr.c_str(), &addr) == 1) {
            char buf[128] = { 0 };
            char* p = buf;
            for (int i = 15; i >= 0; --i) {
                BYTE b = addr.s6_addr[i];
                int low = b & 0x0F;
                int high = (b >> 4) & 0x0F;
                p += snprintf(p, buf + sizeof(buf) - p, "%x.%x.", low, high);
            }
            snprintf(p, buf + sizeof(buf) - p, "ip6.arpa");
            return std::string(buf);
        }
        return ipStr;
    }
};

// ============================================================================
// 2. SYSTEM DNS LOCATOR
// ============================================================================

class SystemDnsLocator {
public:
    static std::string GetPrimaryDnsServer() {
        ULONG outBufLen = sizeof(FIXED_INFO);
        std::vector<BYTE> buffer(outBufLen);
        FIXED_INFO* pFixedInfo = reinterpret_cast<FIXED_INFO*>(buffer.data());

        if (GetNetworkParams(pFixedInfo, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
            buffer.resize(outBufLen);
            pFixedInfo = reinterpret_cast<FIXED_INFO*>(buffer.data());
        }

        std::string dnsIp = "8.8.8.8";
        if (GetNetworkParams(pFixedInfo, &outBufLen) == NO_ERROR) {
            std::string candidate = pFixedInfo->DnsServerList.IpAddress.String;
            if (!candidate.empty() && candidate != "0.0.0.0") {
                dnsIp = candidate;
            }
        }

        return dnsIp;
    }
};

// ============================================================================
// 3. DNS RESOLVER ENGINE
// ============================================================================

class ResolveEngine {
private:
    std::string m_dnsServer;
    std::string m_queryTypeStr;
    WORD m_queryType;
    bool m_debug;
    std::map<std::string, WORD> m_queryTypes;

public:
    ResolveEngine() : m_queryTypeStr("A"), m_queryType(DNS_TYPE_A), m_debug(false) {
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

    void SetServer(const std::string& serverInput) {
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

    std::string GetServer() const { return m_dnsServer; }

    bool SetQueryType(const std::string& type) {
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

    std::string GetQueryType() const { return m_queryTypeStr; }

    void SetDebug(bool debug) { m_debug = debug; }
    bool GetDebug() const { return m_debug; }

    bool ExecuteQuery(const std::string& targetInput) {
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
};

// ============================================================================
// 4. INTERACTIVE REPL SESSION
// ============================================================================

class InteractiveSession {
public:
    static void Run(ResolveEngine& engine) {
        std::cout << "Default Server:  " << engine.GetServer() << "\n"
                  << "Address:  " << engine.GetServer() << "#53\n\n";

        std::string line;
        while (true) {
            std::cout << "> ";
            if (!std::getline(std::cin, line)) break;

            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            if (line.empty()) continue;

            if (line == "exit" || line == "quit") {
                break;
            } else if (line == "help" || line == "?") {
                std::cout << "Interactive Commands:\n"
                          << "  <host|ip>        Perform DNS query.\n"
                          << "  server <ip|name> Change DNS server.\n"
                          << "  set type=TYPE    Set query type (A, AAAA, MX, NS, PTR, SOA, TXT, ANY).\n"
                          << "  set debug        Enable debug logging.\n"
                          << "  set nodebug      Disable debug logging.\n"
                          << "  exit             Exit shell.\n\n";
            } else if (line.rfind("server ", 0) == 0) {
                std::string serverInput = line.substr(7);
                engine.SetServer(serverInput);
                std::cout << "Default Server:  " << engine.GetServer() << "\n"
                          << "Address:  " << engine.GetServer() << "#53\n\n";
            } else if (line.rfind("set type=", 0) == 0 || line.rfind("set q=", 0) == 0) {
                size_t eqPos = line.find('=');
                std::string qType = line.substr(eqPos + 1);
                if (!engine.SetQueryType(qType)) {
                    std::cout << "*** Unknown query type: " << qType << "\n";
                }
            } else if (line == "set debug") {
                engine.SetDebug(true);
            } else if (line == "set nodebug") {
                engine.SetDebug(false);
            } else {
                engine.ExecuteQuery(line);
                std::cout << "\n";
            }
        }
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class ResolveApplication {
public:
        void PrintHelp(const wchar_t* progName) const {
           std::wcout << LR"HELP(resolve(1)               CrossShell for UNIX Reference Manual                  resolve(1)

    NAME
        resolve - query DNS records and reverse DNS names

    SYNOPSIS
        resolve [OPTIONS] [HOST|IP] [DNS_SERVER]

    DESCRIPTION
        Resolves hostnames, IPv4 addresses, and IPv6 addresses using DNS. With no
        target or with '-' as the target, resolve starts an interactive session.

    OPTIONS
        -type=TYPE, -querytype=TYPE
            Select A, AAAA, MX, NS, PTR, SOA, TXT, or ANY records.
        -debug                 Enable verbose DNS header and query output.
        -nodebug               Disable debug output (default).
        -?, -h, --help         Display this comprehensive reference manual and exit.

    INTERACTIVE COMMANDS
        HOST|IP                Perform a query.
        server IP|NAME         Change the DNS server.
        set type=TYPE          Change the query type.
        set q=TYPE             Change the query type.
        set debug/nodebug      Toggle debug output.
        help, ?, exit, quit    Display help or leave the session.

    EXAMPLES
        resolve example.com
        resolve -type=MX example.com 8.8.8.8
        resolve 8.8.8.8
        resolve -

    EXIT STATUS
        0          Help, interactive termination, or completed query.
        1          Winsock initialization failure.

    CrossShell for UNIX                                                      resolve(1)
    )HELP";
           return;

        std::wcout << L"Usage: " << progName << L" [-option ...] [host-to-find | - [server]]\n\n"
                   << L"Queries DNS domain name servers interactively or in non-interactive mode.\n\n"
                   << L"Non-Interactive Flags:\n"
                   << L"  -type=TYPE      Set record query type (A, AAAA, MX, NS, PTR, SOA, TXT, ANY).\n"
                   << L"  -querytype=TYPE Synonym for -type.\n"
                   << L"  -debug          Enable verbose DNS debug header logging.\n"
                   << L"  -nodebug        Disable verbose debug output (default).\n"
                   << L"  -?, -h, --help  Display this comprehensive help menu.\n\n"
                   << L"Interactive Mode Commands (type 'resolve' with no args to launch):\n"
                   << L"  <host|ip>        Look up the specified hostname or IP address.\n"
                   << L"  server <ip|name> Change default DNS server to IP or hostname.\n"
                   << L"  set type=TYPE    Change record query type (A, MX, NS, PTR, etc.).\n"
                   << L"  set debug        Enable debug logging in interactive mode.\n"
                   << L"  set nodebug      Disable debug logging.\n"
                   << L"  exit / quit      Exit interactive mode.\n"
                   << L"  help / ?         Show interactive help.\n\n";
    }

    int Run(int argc, wchar_t* argv[]) {
        SetConsoleOutputCP(CP_UTF8);
        setlocale(LC_ALL, ".UTF-8");

        WinsockScope winsock;
        if (!winsock.IsInitialized()) {
            std::cerr << "Error: WSAStartup failed.\n";
            return 1;
        }

        ResolveEngine engine;
        std::string targetHost = "";

        for (int i = 1; i < argc; ++i) {
            std::string arg = StringEncoding::WideToUtf8(argv[i]);
            if (arg == "-?" || arg == "-h" || arg == "--help") {
                PrintHelp(argv[0]);
                return 0;
            } else if (arg.rfind("-type=", 0) == 0) {
                engine.SetQueryType(arg.substr(6));
            } else if (arg.rfind("-querytype=", 0) == 0) {
                engine.SetQueryType(arg.substr(11));
            } else if (arg == "-debug") {
                engine.SetDebug(true);
            } else if (arg == "-nodebug") {
                engine.SetDebug(false);
            } else if (!arg.empty() && arg[0] != '-') {
                if (targetHost.empty()) {
                    targetHost = arg;
                } else {
                    engine.SetServer(arg);
                }
            }
        }

        if (targetHost.empty() || targetHost == "-") {
            InteractiveSession::Run(engine);
        } else {
            engine.ExecuteQuery(targetHost);
        }

        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    ResolveApplication app;
    return app.Run(argc, argv);
}
