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
 * SINGLE FILE INDEX: prtconf.cpp
 * ============================================================================
 * WinPrtconf - Object-Oriented System Hardware & Device Configuration Reporter
 * Specification: C++17 / Solaris-style | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [DATA MODEL & OPTIONS] ................ SystemConfigSummary, PrtconfOptions classes
 * 2. [STRUCTURED OUTPUT REPORTER] .......... PrtconfReporter class
 * 3. [SYSTEM CONFIG SCANNER ENGINE] ........ SystemConfigScanner, PrtconfEngine classes
 * 4. [APPLICATION CONTROLLER] .............. PrtconfApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sysinfoapi.h>
#include <setupapi.h>
#include <initguid.h>
#include <devpkey.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <memory>

#pragma comment(lib, "setupapi.lib")

// ============================================================================
// 1. DATA MODEL & OPTIONS
// ============================================================================

struct SystemConfigSummary {
    std::string systemModel{"Windows Host"};
    std::string nodeName{"unknown"};
    std::string kernelArchitecture{"unknown"};
    DWORD numProcessors{0};
    uint64_t totalMemoryMb{0};
    std::string osLevel;
    DWORD totalDevices{0};
    std::map<std::string, int> classCounts;
};

class PrtconfOptions {
public:
    bool showDevices{false};

    static void printHelp() {
        std::cout << R"(prtconf(1)                CrossShell for UNIX Reference Manual                prtconf(1)

    NAME
        prtconf - print system hardware and configuration summary

    SYNOPSIS
        prtconf [OPTIONS]

    DESCRIPTION
        prtconf displays system configuration information including host model,
        node name, processor architecture, CPU count, total installed memory,
        operating system build, and hardware device counts.

    OPTIONS
        -v
            Verbose mode; display device count breakdown grouped by setup class.

        -h, --help
            Display this reference manual and exit.

        -V, --version
            Display version information and exit.

    EXAMPLES
        prtconf
            Print standard system configuration summary.

        prtconf -v
            Print configuration summary with verbose device-class breakdown.

    CrossShell for UNIX                                                          prtconf(1)
)";
    }

    static void printVersion() {
        std::cout << "prtconf 1.0.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], PrtconfOptions& opt) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"-v") {
                opt.showDevices = true;
            } else if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                printHelp();
                std::exit(0);
            } else if (arg == L"-V" || arg == L"--version") {
                printVersion();
                std::exit(0);
            } else {
                std::wcerr << L"prtconf: unknown option '" << arg << L"'\n";
                return false;
            }
        }
        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class PrtconfReporter {
public:
    static void report(const SystemConfigSummary& summary, bool showDevices) {
        std::cout << "System Model:        " << summary.systemModel << "\n";
        std::cout << "Node Name:           " << summary.nodeName << "\n";
        std::cout << "Kernel Architecture: " << summary.kernelArchitecture << "\n";
        std::cout << "Processors:          " << summary.numProcessors << "\n";
        std::cout << "Memory Size:         " << summary.totalMemoryMb << " MB\n";
        std::cout << "OS Level:            " << summary.osLevel << "\n";
        std::cout << "Devices Present:     " << summary.totalDevices << "\n";

        if (showDevices && !summary.classCounts.empty()) {
            std::cout << "\nDevice Class Summary:\n";
            std::cout << "--------------------  -----\n";
            for (const auto& it : summary.classCounts) {
                std::cout << "  " << std::setw(20) << std::left << it.first << " " << it.second << "\n";
            }
        }
    }
};

// ============================================================================
// 3. SYSTEM CONFIG SCANNER ENGINE
// ============================================================================

class SystemConfigScanner {
private:
    static std::string toUtf8(const std::wstring& w) {
        if (w.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return "";
        std::string out(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &out[0], size, nullptr, nullptr);
        return out;
    }

    static std::wstring getPropString(HDEVINFO info, SP_DEVINFO_DATA& dev, const DEVPROPKEY& key) {
        DEVPROPTYPE type = 0;
        DWORD needed = 0;
        SetupDiGetDevicePropertyW(info, &dev, &key, &type, nullptr, 0, &needed, 0);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || needed == 0) {
            return L"";
        }

        std::vector<BYTE> buf(needed);
        if (!SetupDiGetDevicePropertyW(info, &dev, &key, &type, buf.data(), needed, nullptr, 0)) {
            return L"";
        }

        if (type == DEVPROP_TYPE_STRING || type == DEVPROP_TYPE_STRING_LIST) {
            return std::wstring(reinterpret_cast<const wchar_t*>(buf.data()));
        }
        return L"";
    }

    static std::string archWord(WORD arch) {
        switch (arch) {
            case PROCESSOR_ARCHITECTURE_AMD64: return "x86_64";
            case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
            case PROCESSOR_ARCHITECTURE_ARM64: return "arm64";
            case PROCESSOR_ARCHITECTURE_ARM:   return "arm";
            default: return "unknown";
        }
    }

public:
    static SystemConfigSummary collect() {
        SystemConfigSummary summary;

        wchar_t computerName[256]{};
        DWORD computerNameSize = static_cast<DWORD>(std::size(computerName));
        if (GetComputerNameExW(ComputerNamePhysicalDnsHostname, computerName, &computerNameSize)) {
            summary.nodeName = toUtf8(computerName);
        }

        SYSTEM_INFO si{};
        GetNativeSystemInfo(&si);
        summary.kernelArchitecture = archWord(si.wProcessorArchitecture);
        summary.numProcessors = si.dwNumberOfProcessors;

        MEMORYSTATUSEX mem{};
        mem.dwLength = sizeof(mem);
        GlobalMemoryStatusEx(&mem);
        summary.totalMemoryMb = mem.ullTotalPhys / (1024ULL * 1024ULL);

        OSVERSIONINFOW osv{};
        osv.dwOSVersionInfoSize = sizeof(osv);
        GetVersionExW(&osv);
        summary.osLevel = std::to_string(osv.dwMajorVersion) + "." +
                          std::to_string(osv.dwMinorVersion) + " (Build " +
                          std::to_string(osv.dwBuildNumber) + ")";

        HDEVINFO info = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
        if (info != INVALID_HANDLE_VALUE) {
            DWORD idx = 0;
            SP_DEVINFO_DATA dev{};
            dev.cbSize = sizeof(dev);

            while (SetupDiEnumDeviceInfo(info, idx++, &dev)) {
                std::wstring devClass = getPropString(info, dev, DEVPKEY_Device_Class);
                if (devClass.empty()) devClass = L"Unknown";
                ++summary.classCounts[toUtf8(devClass)];
                ++summary.totalDevices;
            }
            SetupDiDestroyDeviceInfoList(info);
        }

        return summary;
    }
};

class PrtconfEngine {
private:
    PrtconfOptions options;

public:
    explicit PrtconfEngine(PrtconfOptions opts) : options(opts) {}

    int execute() {
        SetConsoleOutputCP(CP_UTF8);
        SystemConfigSummary summary = SystemConfigScanner::collect();
        PrtconfReporter::report(summary, options.showDevices);
        return 0;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class PrtconfApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        PrtconfOptions options;
        if (!PrtconfOptions::parse(argc, argv, options)) {
            std::cerr << "Try 'prtconf --help' for usage.\n";
            return 2;
        }
        PrtconfEngine engine(options);
        return engine.execute();
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return PrtconfApp::run(argc, argv);
}
