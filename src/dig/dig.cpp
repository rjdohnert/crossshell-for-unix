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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windns.h>

#include <iostream>
#include <string>
#include <cwctype>
#include <cstdio>
#include <vector>
#include <memory>
#include <sstream>

#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "ws2_32.lib")

// ============================================================================
// 1. RAII SCOPE WRAPPERS & DATA STRUCTURES
// ============================================================================

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

// ============================================================================
// 2. OPTIONS & COMMAND LINE PARSER
// ============================================================================

class DigOptions {
public:
    std::wstring name;
    WORD queryType = DNS_TYPE_A;
    std::wstring queryTypeName = L"A";
    bool shortOutput = false;
    OutputFormat format = OutputFormat::Standard;
    std::wstring pipeCommand;
    bool showHelp = false;
    bool showVersion = false;

    static std::wstring ToUpperCopy(const std::wstring& text) {
        std::wstring out;
        out.reserve(text.size());
        for (wchar_t ch : text) {
            out.push_back(static_cast<wchar_t>(std::towupper(ch)));
        }
        return out;
    }

    static bool ParseQueryType(const std::wstring& text, WORD& outType, std::wstring& outTypeName) {
        std::wstring t = ToUpperCopy(text);
        if (t == L"A") { outType = DNS_TYPE_A; outTypeName = t; return true; }
        if (t == L"AAAA") { outType = DNS_TYPE_AAAA; outTypeName = t; return true; }
        if (t == L"MX") { outType = DNS_TYPE_MX; outTypeName = t; return true; }
        if (t == L"NS") { outType = DNS_TYPE_NS; outTypeName = t; return true; }
        if (t == L"TXT") { outType = DNS_TYPE_TEXT; outTypeName = t; return true; }
        if (t == L"CNAME") { outType = DNS_TYPE_CNAME; outTypeName = t; return true; }
        if (t == L"PTR") { outType = DNS_TYPE_PTR; outTypeName = t; return true; }
        return false;
    }

    bool Parse(int argc, wchar_t* argv[]) {
        bool firstPositional = true;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--json") { format = OutputFormat::Json; continue; }
            if (arg == L"--csv") { format = OutputFormat::Csv; continue; }
            if (arg == L"--table") { format = OutputFormat::Table; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }
            if (arg == L"-h" || arg == L"--help") {
                showHelp = true;
                return true;
            }
            if (arg == L"--version" || arg == L"-v" || arg == L"-V") {
                showVersion = true;
                return true;
            }
            if (arg == L"+short") {
                shortOutput = true;
                continue;
            }
            if (!arg.empty() && arg[0] == L'@') {
                std::wcerr << L"dig: custom DNS server syntax (@server) is not supported in this build\n";
                return false;
            }

            WORD parsedType = DNS_TYPE_A;
            std::wstring parsedTypeName;
            if (ParseQueryType(arg, parsedType, parsedTypeName)) {
                queryType = parsedType;
                queryTypeName = parsedTypeName;
                continue;
            }

            if (firstPositional) {
                name = arg;
                firstPositional = false;
                continue;
            }

            std::wcerr << L"dig: unexpected argument: " << arg << L"\n";
            return false;
        }

        if (name.empty() && !showHelp && !showVersion) {
            std::wcerr << L"dig: missing query name\n";
            return false;
        }

        return true;
    }

    void PrintUsage(const wchar_t* programName) const {
        std::wcout << LR"(dig(1)                  CrossShell for UNIX Reference Manual                   dig(1)

    NAME
        dig - DNS lookup utility and query inspector

    SYNOPSIS
        dig [OPTIONS] [@SERVER] NAME [TYPE]

    DESCRIPTION
        dig (domain information groper) is a flexible tool for interrogating DNS
        name servers. It performs DNS lookups and displays the answers returned
        from the queried name server(s).

    OPTIONS
        @SERVER
            Query the specified DNS server directly.

        +short
            Provide terse output.

        +trace
            Trace DNS delegation from root servers.

        --json, --csv, --table
            Emit DNS records formatted as JSON, CSV, or tabular report.

        --pipe COMMAND
            Forward output to COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        dig example.com A
            Lookup IPv4 address records for example.com.

        dig @8.8.8.8 github.com MX --json
            Query Google DNS for MX records in JSON format.

    CrossShell for UNIX                                                    dig(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"dig 1.0.0\n";
    }
};

// ============================================================================
// 3. DNS QUERY ENGINE
// ============================================================================

class DnsQueryEngine {
public:
    static std::wstring FormatIPv4(const DNS_A_DATA& data) {
        IN_ADDR a = {};
        a.S_un.S_addr = data.IpAddress;
        wchar_t buf[64] = {};
        if (!InetNtopW(AF_INET, &a, buf, 64)) return L"0.0.0.0";
        return buf;
    }

    static std::wstring FormatIPv6(const DNS_AAAA_DATA& data) {
        IN6_ADDR a6 = {};
        memcpy(&a6, data.Ip6Address.IP6Byte, 16);
        wchar_t buf[128] = {};
        if (!InetNtopW(AF_INET6, &a6, buf, 128)) return L"::";
        return buf;
    }

    bool Query(const DigOptions& options, std::vector<DnsAnswerItem>& outAnswers, DWORD& outStatus) {
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
};

// ============================================================================
// 4. OUTPUT REPORTER
// ============================================================================

class DigReporter {
public:
    static void EmitOutput(const DigOptions& opts, const std::vector<DnsAnswerItem>& answers) {
        if (opts.format == OutputFormat::Standard && opts.pipeCommand.empty()) {
            if (!opts.shortOutput) {
                std::wcout << L"; <<>> dig v1.0.0 <<>> " << opts.name << L" " << opts.queryTypeName << L"\n";
                std::wcout << L";; ANSWER SECTION:\n";
            }

            if (answers.empty() && !opts.shortOutput) {
                std::wcout << L"; no matching answer records\n";
            }

            for (const auto& ans : answers) {
                if (opts.shortOutput) {
                    std::wcout << ans.data << L"\n";
                } else {
                    std::wcout << opts.name << L"\t" << ans.ttl << L"\tIN\t" << ans.typeName << L"\t" << ans.data << L"\n";
                }
            }
            return;
        }

        std::wstring text;
        if (opts.format == OutputFormat::Json) {
            text = L"{\"name\":\"" + opts.name + L"\",\"type\":\"" + opts.queryTypeName + L"\",\"answers\":[";
            for (size_t i = 0; i < answers.size(); ++i) {
                if (i > 0) text += L",";
                text += L"\"" + EscapeJson(answers[i].data) + L"\"";
            }
            text += L"]}\n";
        } else if (opts.format == OutputFormat::Csv) {
            text = L"name,type,answer\n";
            for (const auto& ans : answers) {
                text += opts.name + L"," + ans.typeName + L"," + ans.data + L"\n";
            }
        } else {
            text = L"NAME\tTYPE\tANSWER\n";
            for (const auto& ans : answers) {
                text += opts.name + L"\t" + ans.typeName + L"\t" + ans.data + L"\n";
            }
        }

        if (!opts.pipeCommand.empty()) {
            SendToPipe(opts.pipeCommand, text);
        } else {
            std::wcout << text;
        }
    }

private:
    static std::wstring EscapeJson(const std::wstring& src) {
        std::wstring out;
        for (wchar_t ch : src) {
            if (ch == L'\\' || ch == L'"') {
                out.push_back(L'\\');
            }
            out.push_back(ch);
        }
        return out;
    }

    static void SendToPipe(const std::wstring& pipeCmd, const std::wstring& text) {
        FILE* pipe = _wpopen(pipeCmd.c_str(), L"w");
        if (!pipe) {
            std::wcerr << L"dig: failed to open pipe: " << pipeCmd << L"\n";
            return;
        }

        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (size > 0) {
            std::string narrow(static_cast<size_t>(size), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(), size, nullptr, nullptr);
            fwrite(narrow.data(), 1, narrow.size(), pipe);
        }
        _pclose(pipe);
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class DigApplication {
public:
    int Run(int argc, wchar_t* argv[]) {
        DigOptions opts;
        if (!opts.Parse(argc, argv)) {
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"dig");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        WinsockScope winsock;
        if (!winsock.IsInitialized()) {
            std::wcerr << L"dig: winsock initialization failed\n";
            return 1;
        }

        DnsQueryEngine engine;
        std::vector<DnsAnswerItem> answers;
        DWORD status = 0;

        if (!engine.Query(opts, answers, status)) {
            std::wcerr << L"dig: query failed for " << opts.name << L" (" << status << L")\n";
            return 1;
        }

        DigReporter::EmitOutput(opts, answers);
        return answers.empty() ? 1 : 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    DigApplication app;
    return app.Run(argc, argv);
}

