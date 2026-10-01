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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <string>
#include <vector>
#include <memory>

// ============================================================================
// 1. SYSTEM NODE MANAGER (DOMAIN ENGINE)
// ============================================================================

enum class NameQueryMode {
    ShortName,
    DomainOnly,
    FullyQualified
};

class SystemNodeManager {
public:
    static std::wstring QueryName(NameQueryMode mode) {
        COMPUTER_NAME_FORMAT format = ComputerNameDnsFullyQualified;
        switch (mode) {
            case NameQueryMode::ShortName:
                format = ComputerNameDnsHostname;
                break;
            case NameQueryMode::DomainOnly:
                format = ComputerNameDnsDomain;
                break;
            case NameQueryMode::FullyQualified:
            default:
                format = ComputerNameDnsFullyQualified;
                break;
        }

        DWORD size = 0;
        GetComputerNameExW(format, nullptr, &size);
        if (size == 0) {
            return L"";
        }

        std::wstring buffer(size, L'\0');
        if (GetComputerNameExW(format, &buffer[0], &size)) {
            buffer.resize(size);
            return buffer;
        }
        return L"";
    }

    static bool SetNodeName(const std::wstring& newHostname, DWORD& outError) {
        outError = 0;
        if (SetComputerNameExW(ComputerNamePhysicalDnsHostname, newHostname.c_str())) {
            return true;
        }
        outError = GetLastError();
        return false;
    }
};

// ============================================================================
// 2. OPTIONS & COMMAND LINE PARSER
// ============================================================================

class NodenameOptions {
public:
    NameQueryMode mode = NameQueryMode::FullyQualified;
    std::wstring newHostname;
    bool hasNewHostname = false;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--") {
                for (int j = i + 1; j < argc; ++j) {
                    if (!hasNewHostname) {
                        newHostname = argv[j] ? argv[j] : L"";
                        hasNewHostname = true;
                    } else {
                        std::wcerr << L"nodename: too many arguments\n";
                        return false;
                    }
                }
                break;
            } else if (arg == L"-h" || arg == L"--help") {
                showHelp = true;
                return true;
            } else if (arg == L"--version" || arg == L"-V") {
                showVersion = true;
                return true;
            } else if (arg == L"-s" || arg == L"--short") {
                mode = NameQueryMode::ShortName;
            } else if (arg == L"-d" || arg == L"--domain") {
                mode = NameQueryMode::DomainOnly;
            } else if (arg == L"-f" || arg == L"--fqdn") {
                mode = NameQueryMode::FullyQualified;
            } else if (arg.size() > 1 && arg[0] == L'-') {
                for (size_t j = 1; j < arg.size(); ++j) {
                    switch (arg[j]) {
                        case L's':
                            mode = NameQueryMode::ShortName;
                            break;
                        case L'd':
                            mode = NameQueryMode::DomainOnly;
                            break;
                        case L'f':
                            mode = NameQueryMode::FullyQualified;
                            break;
                        default:
                            std::wcerr << L"nodename: unknown option -- " << arg[j] << L"\n";
                            return false;
                    }
                }
            } else {
                if (hasNewHostname) {
                    std::wcerr << L"nodename: too many arguments\n";
                    return false;
                }
                newHostname = arg;
                hasNewHostname = true;
            }
        }
        return true;
    }

    void PrintUsage(const wchar_t* progName) const {
        std::wcout << L"Usage: " << (progName ? progName : L"nodename") << L" [-f] [-s | -d] [name-of-host]\n"
                   << L"  -s, --short       print the short host name\n"
                   << L"  -d, --domain      print the DNS domain name\n"
                   << L"  -f, --fqdn        print the fully qualified domain name\n"
                   << L"  -h, --help        display this help and exit\n"
                   << L"      --version     output version information and exit\n"
                   << L"      --            end of options\n";
    }

    void PrintVersion() const {
        std::wcout << L"nodename 1.0.0\n";
    }
};

// ============================================================================
// 3. APPLICATION CONTROLLER
// ============================================================================

class NodenameApplication {
public:
    int Run(int argc, wchar_t* argv[]) {
        NodenameOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintUsage(argc > 0 ? argv[0] : L"nodename");
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage(argc > 0 ? argv[0] : L"nodename");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        if (opts.hasNewHostname) {
            DWORD err = 0;
            if (SystemNodeManager::SetNodeName(opts.newHostname, err)) {
                std::wcout << L"Hostname successfully changed to '" << opts.newHostname << L"'.\n";
                std::wcout << L"You must restart the computer for the changes to take effect.\n";
                return 0;
            } else {
                std::wcerr << L"nodename: failed to set hostname. Error code: " << err << L"\n";
                if (err == ERROR_ACCESS_DENIED) {
                    std::wcerr << L"Error: Access Denied. Please run this command as Administrator.\n";
                }
                return 1;
            }
        }

        std::wstring name = SystemNodeManager::QueryName(opts.mode);
        if (name.empty() && opts.mode != NameQueryMode::DomainOnly) {
            std::wcerr << L"nodename: failed to retrieve hostname.\n";
            return 1;
        }

        std::wcout << name << std::endl;
        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    NodenameApplication app;
    return app.Run(argc, argv);
}
