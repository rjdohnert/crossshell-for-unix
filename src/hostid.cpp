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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>
#include <memory>
#include <fcntl.h>
#include <io.h>
#include <cstdio>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Advapi32.lib")

// ============================================================================
// 1. RAII SCOPES & HARDWARE IDENTIFIERS
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

class RegistryReader {
public:
    static std::wstring GetMachineGuid() {
        HKEY hKey = nullptr;
        wchar_t guidBuf[256] = { 0 };
        DWORD dwSize = sizeof(guidBuf);

        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
            RegQueryValueExW(hKey, L"MachineGuid", nullptr, nullptr, reinterpret_cast<LPBYTE>(guidBuf), &dwSize);
            RegCloseKey(hKey);
        }
        return std::wstring(guidBuf);
    }
};

class HostIdentifierGenerator {
public:
    static uint32_t HashString32(const std::wstring& str) {
        uint32_t hash = 2166136261u;
        for (wchar_t c : str) {
            hash ^= static_cast<uint32_t>(c);
            hash *= 16777619u;
        }
        return hash;
    }

    static uint32_t ComputeHostId() {
        WinsockScope winsock;
        if (winsock.IsInitialized()) {
            char hostname[256] = { 0 };
            if (gethostname(hostname, sizeof(hostname)) == 0) {
                addrinfo hints = {};
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_STREAM;

                addrinfo* result = nullptr;
                if (getaddrinfo(hostname, nullptr, &hints, &result) == 0 && result != nullptr) {
                    sockaddr_in* sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(result->ai_addr);
                    uint32_t ip = sockaddr_ipv4->sin_addr.s_addr;
                    freeaddrinfo(result);

                    if (ip != 0 && ip != htonl(INADDR_LOOPBACK)) {
                        unsigned char* b = reinterpret_cast<unsigned char*>(&ip);
                        return (static_cast<uint32_t>(b[2]) << 24) |
                               (static_cast<uint32_t>(b[3]) << 16) |
                               (static_cast<uint32_t>(b[0]) << 8)  |
                               (static_cast<uint32_t>(b[1]));
                    }
                }
            }
        }

        std::wstring machineGuid = RegistryReader::GetMachineGuid();
        if (!machineGuid.empty()) {
            return HashString32(machineGuid);
        }

        wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
        DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
        GetComputerNameW(computerName, &size);
        return HashString32(computerName);
    }
};

// ============================================================================
// 2. OPTIONS & COMMAND LINE PARSER
// ============================================================================

enum class OutputFormat {
    Standard = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

class HostidOptions {
public:
    OutputFormat format = OutputFormat::Standard;
    std::string pipeCommand;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";
            if (arg == "--json") {
                format = OutputFormat::Json;
            } else if (arg == "--csv") {
                format = OutputFormat::Csv;
            } else if (arg == "--table") {
                format = OutputFormat::Table;
            } else if (arg == "--pipe" && i + 1 < argc) {
                pipeCommand = argv[++i];
            } else if (arg == "--help" || arg == "-h" || arg == "/?") {
                showHelp = true;
                return true;
            } else if (arg == "--version" || arg == "-V") {
                showVersion = true;
                return true;
            } else if (arg == "--") {
                for (++i; i < argc; ++i) {
                    std::cerr << "hostid: extra operand '" << argv[i] << "'\n";
                    return false;
                }
                break;
            } else {
                std::cerr << "hostid: invalid option -- '" << arg << "'\n";
                return false;
            }
        }
        return true;
    }

    void PrintUsage() const {
        std::cout << R"(hostid(1)               CrossShell for UNIX Reference Manual                hostid(1)

    NAME
        hostid - print the numeric identifier for the current host

    SYNOPSIS
        hostid [OPTIONS]

    DESCRIPTION
        hostid prints the numeric identifier (in hexadecimal) for the current host,
        derived from the machine GUID, motherboard UUID, or network interface MAC.

    OPTIONS
        --json, --csv, --table
            Output host identifier as JSON, CSV, or table.

        --pipe COMMAND
            Route output into COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        hostid
            Display host identifier in hex format.

    CrossShell for UNIX                                                 hostid(1)
)";
    }

    void PrintVersion() const {
        std::cout << "hostid 1.0.0\n";
    }
};

// ============================================================================
// 3. OUTPUT REPORTER
// ============================================================================

class HostidReporter {
public:
    static void Emit(const HostidOptions& opts, uint32_t hostId) {
        std::ostringstream output;
        output << std::setw(8) << std::setfill('0') << std::hex << hostId;
        std::string hexStr = output.str();

        std::string text;
        if (opts.format == OutputFormat::Json) {
            text = "{\"hostid\":\"" + hexStr + "\"}\n";
        } else if (opts.format == OutputFormat::Csv) {
            text = "hostid\n" + hexStr + "\n";
        } else if (opts.format == OutputFormat::Table) {
            text = "HOSTID\n------\n" + hexStr + "\n";
        } else {
            text = hexStr + "\n";
        }

        if (!opts.pipeCommand.empty()) {
            FILE* pipe = _popen(opts.pipeCommand.c_str(), "w");
            if (pipe) {
                fwrite(text.data(), 1, text.size(), pipe);
                _pclose(pipe);
            }
        } else {
            std::cout << text;
        }
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class HostidApplication {
public:
    int Run(int argc, char* argv[]) {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        HostidOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintUsage();
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage();
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        uint32_t id = HostIdentifierGenerator::ComputeHostId();
        HostidReporter::Emit(opts, id);
        return 0;
    }
};

int main(int argc, char* argv[]) {
    HostidApplication app;
    return app.Run(argc, argv);
}
