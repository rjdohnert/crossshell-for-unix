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
#include <fcntl.h>
#include <io.h>
#include <string>
#include <sstream>
#include <vector>
#include <memory>
#include <cstdio>

#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "ws2_32.lib")

// ============================================================================
// 1. RAII SCOPES & DATA STRUCTURES
// ============================================================================

class NativePipeConfigurator {
public:
    static void Configure() {
        HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD consoleMode = 0;
        const bool stdoutConsole = (GetFileType(stdoutHandle) == FILE_TYPE_CHAR) && GetConsoleMode(stdoutHandle, &consoleMode);
        _setmode(_fileno(stdout), stdoutConsole ? _O_U16TEXT : _O_U8TEXT);
        _setmode(_fileno(stderr), _O_U8TEXT);
    }
};

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

enum class OutputFormat {
    Standard = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

struct ResolvedRecord {
    std::wstring query;
    std::wstring type;
    std::wstring value;
    std::wstring formattedLine;
};

// ============================================================================
// 2. OPTIONS & COMMAND LINE PARSER
// ============================================================================

class HostOptions {
public:
    std::wstring target;
    OutputFormat format = OutputFormat::Standard;
    std::wstring pipeCommand;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring option = argv[i] ? argv[i] : L"";
            if (option == L"--json") {
                format = OutputFormat::Json;
            } else if (option == L"--csv") {
                format = OutputFormat::Csv;
            } else if (option == L"--table") {
                format = OutputFormat::Table;
            } else if (option == L"--pipe" && i + 1 < argc) {
                pipeCommand = argv[++i];
            } else if (option == L"-h" || option == L"--help") {
                showHelp = true;
                return true;
            } else if (option == L"-v" || option == L"-V" || option == L"--version") {
                showVersion = true;
                return true;
            } else if (target.empty() && !option.empty() && option[0] != L'-') {
                target = option;
            } else if (!option.empty() && option[0] == L'-') {
                std::wcerr << L"host: unrecognized option: " << option << L"\n";
                return false;
            }
        }

        if (target.empty() && !showHelp && !showVersion) {
            return false;
        }

        return true;
    }

    void PrintUsage(const wchar_t* programName) const {
        std::wcout << LR"(host(1)                 CrossShell for UNIX Reference Manual                  host(1)

    NAME
        host - DNS lookup utility

    SYNOPSIS
        host [OPTIONS] NAME [SERVER]

    DESCRIPTION
        host is a simple utility for performing DNS lookups. It is normally used
        to convert names to IP addresses and vice versa.

    OPTIONS
        -a, --all
            Query all standard DNS records.

        -t, --type TYPE
            Specify query type (A, AAAA, MX, NS, CNAME, PTR, TXT).

        --json, --csv, --table
            Format DNS query results as JSON, CSV, or table.

        --pipe COMMAND
            Stream output into COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        host example.com
            Lookup IP address for example.com.

        host -t MX google.com 8.8.8.8
            Query Google DNS for MX records.

    CrossShell for UNIX                                                   host(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"host 1.0.0\n";
    }
};

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

// ============================================================================
// 4. OUTPUT REPORTER
// ============================================================================

class HostReporter {
public:
    static void Emit(const HostOptions& opts, const std::vector<ResolvedRecord>& records) {
        if (opts.format == OutputFormat::Standard && opts.pipeCommand.empty()) {
            for (const auto& rec : records) {
                std::wcout << rec.formattedLine << L"\n";
            }
            return;
        }

        std::wstring text;
        if (opts.format == OutputFormat::Json) {
            text = L"[";
            for (size_t i = 0; i < records.size(); ++i) {
                if (i > 0) text += L",";
                text += L"{\"query\":\"" + opts.target + L"\",\"type\":\"" + records[i].type +
                        L"\",\"result\":\"" + records[i].value + L"\"}";
            }
            text += L"]\n";
        } else if (opts.format == OutputFormat::Csv) {
            text = L"query,type,result\n";
            for (const auto& rec : records) {
                text += opts.target + L"," + rec.type + L"," + rec.value + L"\n";
            }
        } else {
            text = L"QUERY\tTYPE\tRESULT\n";
            for (const auto& rec : records) {
                text += opts.target + L"\t" + rec.type + L"\t" + rec.value + L"\n";
            }
        }

        if (!opts.pipeCommand.empty()) {
            SendToPipe(opts.pipeCommand, text);
        } else {
            std::wcout << text;
        }
    }

private:
    static void SendToPipe(const std::wstring& pipeCmd, const std::wstring& text) {
        FILE* pipe = _wpopen(pipeCmd.c_str(), L"w");
        if (!pipe) {
            std::wcerr << L"host: failed to open pipe: " << pipeCmd << L"\n";
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

class HostApplication {
public:
    int Run(int argc, wchar_t* argv[]) {
        NativePipeConfigurator::Configure();

        HostOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"host");
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"host");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        WinsockScope winsock;
        if (!winsock.IsInitialized()) {
            std::wcerr << L"host: winsock initialization failed\n";
            return 1;
        }

        HostResolver resolver;
        std::vector<ResolvedRecord> records;
        if (!resolver.Resolve(opts.target, records)) {
            std::wcerr << L"Host " << opts.target << L" not found\n";
            return 1;
        }

        HostReporter::Emit(opts, records);
        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    HostApplication app;
    return app.Run(argc, argv);
}

