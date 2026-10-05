/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
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

/**
 * ============================================================================
 * SINGLE FILE INDEX: uname.cpp
 * ============================================================================
 * WinUname - Object-Oriented System Identification & Kernel Architecture Query
 * Specification: POSIX.1-2017 / BSD / Windows NT | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & SYSTEM DETAILS DATA] ....... SystemDetails, UnameOptions classes
 * 2. [STRUCTURED OUTPUT REPORTER] .......... UnameReporter class (Text, JSON, CSV, Table, SVr4 -X)
 * 3. [NT KERNEL INFO PROVIDER] ............. NtKernelInfoProvider, HostnameConfigurator
 * 4. [APPLICATION CONTROLLER] .............. UnameApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winternl.h>

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdio>
#include <memory>

#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "Advapi32.lib")

extern "C" NTSTATUS NTAPI RtlGetVersion(PRTL_OSVERSIONINFOW lpVersionInformation);

#define KUSER_SHARED_DATA_VA ((const BYTE*)0x7FFE0000)
#define KUSD_MAJOR_VERSION   (*(const ULONG*)(KUSER_SHARED_DATA_VA + 0x0260))
#define KUSD_MINOR_VERSION   (*(const ULONG*)(KUSER_SHARED_DATA_VA + 0x0264))
#define KUSD_BUILD_NUMBER    (*(const ULONG*)(KUSER_SHARED_DATA_VA + 0x0268))

// ============================================================================
// 1. OPTIONS & SYSTEM DETAILS DATA
// ============================================================================

struct SystemDetails {
    std::wstring sysname;          // -s
    std::wstring nodename;         // -n
    std::wstring release;          // -r
    std::wstring version;          // -v
    std::wstring machine;          // -m
    std::wstring machineId;        // -i
    std::wstring licenseId;        // -l
    std::wstring model;            // -M
    std::wstring marketingEdition;
    DWORD buildNumber{0};
    DWORD ubr{0};
    DWORD numProcessors{0};
};

class UnameOptions {
public:
    bool printSysname{false};
    bool printNodename{false};
    bool printRelease{false};
    bool printVersion{false};
    bool printMachine{false};
    bool printId{false};
    bool printLicense{false};
    bool printModel{false};
    bool printExtended{false};
    std::wstring setHostname;
    int outputFormat{0};
    std::wstring pipeCommand;

    static void printHelp() {
        std::wcout << LR"(UNAME(1)                         User Commands                        UNAME(1)

NAME
    uname v3.1.0 - print or set system information

SYNOPSIS
     uname [-a | -s | -n | -r | -v | -m | -i | -l | -M | -X]
     uname -S nodename

OPTIONS
     -a, --all        Print all core system identification fields in canonical order.
     -s, --sysname    Print the system implementation name (Windows NT).
     -n, --nodename   Print the network node hostname.
     -r, --release    Print the operating system release level (e.g., 10.0.24H2).
     -v, --version    Print the operating system kernel version and build number.
     -m, --machine    Print the hardware machine instruction architecture (e.g., x86_64, arm64).
     -i, --id         Print the hardware machine identifier (Machine GUID / UUID).
     -l, --license    Print the system software license / Windows Product ID number.
     -M, --model      Print the hardware system model and motherboard name.
     -X               Print extended system summary in SVr4 / Solaris format.
     -S nodename      Set the network node hostname (Requires Administrator elevation).
         --json, --csv, --table  Structured output.
         --pipe CMD   Send output through CMD.
     -h, --help       Display this comprehensive help section and exit.
)";
    }

    static void printVersionHeader() {
        std::wcout << L"uname 3.1.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], UnameOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                printHelp();
                std::exit(0);
            } else if (arg == L"--version") {
                printVersionHeader();
                std::exit(0);
            } else if (arg == L"--json") {
                opts.outputFormat = 1;
            } else if (arg == L"--csv") {
                opts.outputFormat = 2;
            } else if (arg == L"--table") {
                opts.outputFormat = 3;
            } else if (arg == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg == L"-a" || arg == L"--all") {
                opts.printSysname = opts.printNodename = opts.printRelease = true;
                opts.printVersion = opts.printMachine = opts.printId = true;
            } else if (arg == L"-s" || arg == L"--sysname" || arg == L"--kernel-name") {
                opts.printSysname = true;
            } else if (arg == L"-n" || arg == L"--nodename") {
                opts.printNodename = true;
            } else if (arg == L"-r" || arg == L"--release" || arg == L"--kernel-release") {
                opts.printRelease = true;
            } else if (arg == L"-v" || arg == L"--kernel-version") {
                opts.printVersion = true;
            } else if (arg == L"-m" || arg == L"--machine" || arg == L"-p" || arg == L"--processor") {
                opts.printMachine = true;
            } else if (arg == L"-i" || arg == L"--id") {
                opts.printId = true;
            } else if (arg == L"-l" || arg == L"--license") {
                opts.printLicense = true;
            } else if (arg == L"-M" || arg == L"--model") {
                opts.printModel = true;
            } else if (arg == L"-X") {
                opts.printExtended = true;
            } else if (arg == L"-S" && i + 1 < argc) {
                opts.setHostname = argv[++i];
            } else if (arg[0] == L'-' && arg.size() > 1) {
                for (size_t j = 1; j < arg.size(); ++j) {
                    wchar_t c = arg[j];
                    if (c == L'a') { opts.printSysname = opts.printNodename = opts.printRelease = opts.printVersion = opts.printMachine = opts.printId = true; }
                    else if (c == L's') opts.printSysname = true;
                    else if (c == L'n') opts.printNodename = true;
                    else if (c == L'r') opts.printRelease = true;
                    else if (c == L'v') opts.printVersion = true;
                    else if (c == L'm' || c == L'p') opts.printMachine = true;
                    else if (c == L'i') opts.printId = true;
                    else if (c == L'l') opts.printLicense = true;
                    else if (c == L'M') opts.printModel = true;
                    else if (c == L'X') opts.printExtended = true;
                    else {
                        std::wcerr << L"uname: unknown option -- " << c << L"\n";
                        return false;
                    }
                }
            } else {
                std::wcerr << L"uname: extra operand " << arg << L"\n";
                return false;
            }
        }

        if (!opts.printSysname && !opts.printNodename && !opts.printRelease &&
            !opts.printVersion && !opts.printMachine && !opts.printId &&
            !opts.printLicense && !opts.printModel && !opts.printExtended && opts.setHostname.empty()) {
            opts.printSysname = true;
        }

        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class UnameReporter {
public:
    static std::string toUtf8(const std::wstring& text) {
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &result[0], size, NULL, NULL);
        return result;
    }

    static int dispatch(const SystemDetails& sys, const UnameOptions& opts) {
        std::wstring text;

        if (opts.printExtended) {
            std::wstringstream ss;
            ss << L"System = " << sys.sysname << L"\n"
               << L"Node = " << sys.nodename << L"\n"
               << L"Release = " << sys.release << L"\n"
               << L"KernelID = " << sys.version << L"\n"
               << L"Machine = " << sys.machine << L"\n"
               << L"Edition = " << sys.marketingEdition << L"\n"
               << L"Model = " << sys.model << L"\n"
               << L"NumCPU = " << sys.numProcessors << L"\n"
               << L"HardwareID = " << (sys.machineId.empty() ? L"N/A" : sys.machineId) << L"\n";
            text = ss.str();
        } else if (opts.outputFormat == 1) {
            text = L"{\"sysname\":\"" + sys.sysname + L"\",\"nodename\":\"" + sys.nodename +
                   L"\",\"release\":\"" + sys.release + L"\",\"version\":\"" + sys.version +
                   L"\",\"machine\":\"" + sys.machine + L"\"}\n";
        } else if (opts.outputFormat == 2) {
            text = L"sysname,nodename,release,version,machine\n\"" +
                   sys.sysname + L"\",\"" + sys.nodename + L"\",\"" + sys.release +
                   L"\",\"" + sys.version + L"\",\"" + sys.machine + L"\"\n";
        } else if (opts.outputFormat == 3) {
            text = L"SYSNAME\tNODENAME\tRELEASE\tVERSION\tMACHINE\n" +
                   sys.sysname + L"\t" + sys.nodename + L"\t" + sys.release +
                   L"\t" + sys.version + L"\t" + sys.machine + L"\n";
        } else {
            std::vector<std::wstring> parts;
            if (opts.printSysname) parts.push_back(sys.sysname);
            if (opts.printNodename) parts.push_back(sys.nodename);
            if (opts.printRelease) parts.push_back(sys.release);
            if (opts.printVersion) parts.push_back(sys.version);
            if (opts.printMachine) parts.push_back(sys.machine);
            if (opts.printId && !sys.machineId.empty()) parts.push_back(sys.machineId);
            if (opts.printLicense && !sys.licenseId.empty()) parts.push_back(sys.licenseId);
            if (opts.printModel && !sys.model.empty()) parts.push_back(sys.model);

            for (size_t i = 0; i < parts.size(); ++i) {
                if (i > 0) text += L" ";
                text += parts[i];
            }
            text += L"\n";
        }

        if (!opts.pipeCommand.empty()) {
            FILE* pipe = _wpopen(opts.pipeCommand.c_str(), L"w");
            if (!pipe) return 1;
            std::string utf8 = toUtf8(text);
            std::fwrite(utf8.data(), 1, utf8.size(), pipe);
            _pclose(pipe);
        } else {
            std::wcout << text;
        }
        return 0;
    }
};

// ============================================================================
// 3. NT KERNEL INFO PROVIDER
// ============================================================================

class NtKernelInfoProvider {
private:
    static std::wstring queryRegString(HKEY hKey, const wchar_t* valueName) {
        if (!hKey) return L"";
        DWORD bytesNeeded = 0;
        DWORD type = 0;
        LONG status = RegQueryValueExW(hKey, valueName, nullptr, &type, nullptr, &bytesNeeded);
        if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || bytesNeeded == 0) {
            return L"";
        }
        std::vector<wchar_t> buffer((bytesNeeded / sizeof(wchar_t)) + 1, 0);
        status = RegQueryValueExW(hKey, valueName, nullptr, nullptr, reinterpret_cast<LPBYTE>(buffer.data()), &bytesNeeded);
        return (status == ERROR_SUCCESS) ? std::wstring(buffer.data()) : L"";
    }

    static DWORD queryRegDword(HKEY hKey, const wchar_t* valueName, DWORD defaultVal = 0) {
        if (!hKey) return defaultVal;
        DWORD val = 0;
        DWORD size = sizeof(val);
        DWORD type = 0;
        if (RegQueryValueExW(hKey, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS && type == REG_DWORD) {
            return val;
        }
        return defaultVal;
    }

    static std::wstring detectEdition(DWORD major, DWORD minor, DWORD build, bool isServer, const std::wstring& regProductName) {
        if (isServer) {
            if (build >= 26100) return L"Windows Server 2025";
            if (build >= 20348) return L"Windows Server 2022";
            if (build >= 17763) return L"Windows Server 2019";
            if (build >= 14393) return L"Windows Server 2016";
            if (build >= 9600)  return L"Windows Server 2012 R2";
            return regProductName.empty() ? L"Windows Server" : regProductName;
        }

        if (major == 10) {
            if (build >= 22000) {
                std::wstring prod = regProductName;
                size_t pos = prod.find(L"Windows 10");
                if (pos != std::wstring::npos) {
                    prod.replace(pos, 10, L"Windows 11");
                }
                return prod.empty() ? L"Windows 11" : prod;
            }
            return regProductName.empty() ? L"Windows 10" : regProductName;
        }

        return regProductName.empty() ? L"Windows NT" : regProductName;
    }

public:
    static SystemDetails collect() {
        SystemDetails sys;
        sys.sysname = L"Windows NT";

        wchar_t nodeBuf[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
        DWORD nodeSize = sizeof(nodeBuf) / sizeof(nodeBuf[0]);
        if (GetComputerNameExW(ComputerNamePhysicalDnsHostname, nodeBuf, &nodeSize)) {
            sys.nodename = nodeBuf;
        } else {
            sys.nodename = L"localhost";
        }

        DWORD major = KUSD_MAJOR_VERSION;
        DWORD minor = KUSD_MINOR_VERSION;
        DWORD build = KUSD_BUILD_NUMBER;

        if (major == 0) {
            RTL_OSVERSIONINFOW osInfo = { 0 };
            osInfo.dwOSVersionInfoSize = sizeof(osInfo);
            if (RtlGetVersion(&osInfo) == 0) {
                major = osInfo.dwMajorVersion;
                minor = osInfo.dwMinorVersion;
                build = osInfo.dwBuildNumber;
            }
        }

        sys.buildNumber = build;

        HKEY hKeyCurrentVersion = nullptr;
        std::wstring regProductName;
        std::wstring displayVersion;
        std::wstring installType;

        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 
                          0, KEY_READ, &hKeyCurrentVersion) == ERROR_SUCCESS) {
            sys.ubr = queryRegDword(hKeyCurrentVersion, L"UBR", 0);
            installType = queryRegString(hKeyCurrentVersion, L"InstallationType");
            regProductName = queryRegString(hKeyCurrentVersion, L"ProductName");
            displayVersion = queryRegString(hKeyCurrentVersion, L"DisplayVersion");
            if (displayVersion.empty()) {
                displayVersion = queryRegString(hKeyCurrentVersion, L"ReleaseId");
            }
            sys.licenseId = queryRegString(hKeyCurrentVersion, L"ProductId");
            RegCloseKey(hKeyCurrentVersion);
        }

        bool isServer = (_wcsicmp(installType.c_str(), L"Server") == 0);
        sys.marketingEdition = detectEdition(major, minor, build, isServer, regProductName);

        std::wstringstream relStream;
        relStream << major << L"." << minor;
        if (!displayVersion.empty()) {
            relStream << L"." << displayVersion;
        }
        sys.release = relStream.str();

        std::wstringstream verStream;
        verStream << L"Build " << build;
        if (sys.ubr > 0) {
            verStream << L"." << sys.ubr;
        }
        sys.version = verStream.str();

        SYSTEM_INFO si;
        GetNativeSystemInfo(&si);
        sys.numProcessors = si.dwNumberOfProcessors;

        switch (si.wProcessorArchitecture) {
            case PROCESSOR_ARCHITECTURE_AMD64: sys.machine = L"x86_64"; break;
            case PROCESSOR_ARCHITECTURE_ARM64: sys.machine = L"arm64"; break;
            case PROCESSOR_ARCHITECTURE_ARM:   sys.machine = L"arm"; break;
            case PROCESSOR_ARCHITECTURE_INTEL: sys.machine = L"i686"; break;
            default:                           sys.machine = L"unknown"; break;
        }

        HKEY hKeyCrypto = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 
                          0, KEY_READ, &hKeyCrypto) == ERROR_SUCCESS) {
            sys.machineId = queryRegString(hKeyCrypto, L"MachineGuid");
            RegCloseKey(hKeyCrypto);
        }

        HKEY hKeyBios = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", 
                          0, KEY_READ, &hKeyBios) == ERROR_SUCCESS) {
            std::wstring mfg = queryRegString(hKeyBios, L"SystemManufacturer");
            std::wstring prod = queryRegString(hKeyBios, L"SystemProductName");
            if (!mfg.empty() || !prod.empty()) {
                sys.model = mfg + (mfg.empty() || prod.empty() ? L"" : L" ") + prod;
            } else {
                sys.model = L"PC-Compatible Hardware";
            }
            RegCloseKey(hKeyBios);
        } else {
            sys.model = L"PC-Compatible Hardware";
        }

        return sys;
    }
};

class HostnameConfigurator {
public:
    static bool setHostname(const std::wstring& newName) {
        return SetComputerNameExW(ComputerNamePhysicalDnsHostname, newName.c_str()) != 0;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class UnameApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        UnameOptions options;
        if (!UnameOptions::parse(argc, argv, options)) {
            return 1;
        }

        if (!options.setHostname.empty()) {
            if (HostnameConfigurator::setHostname(options.setHostname)) {
                std::wcout << L"uname: hostname successfully updated to '" << options.setHostname << L"'\n";
                return 0;
            } else {
                std::wcerr << L"uname: error setting hostname (requires Administrator elevation, Error: " << GetLastError() << L")\n";
                return 1;
            }
        }

        SystemDetails sys = NtKernelInfoProvider::collect();
        return UnameReporter::dispatch(sys, options);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return UnameApp::run(argc, argv);
}