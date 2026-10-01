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
#include <winsvc.h>
#include <iostream>
#include <string>

#pragma comment(lib, "advapi32.lib")

// ============================================================================
// 1. RAII SERVICE HANDLE & SERVICE CONTROLLER
// ============================================================================

class ScmHandle {
private:
    SC_HANDLE m_handle{nullptr};

public:
    ScmHandle() = default;
    explicit ScmHandle(SC_HANDLE h) : m_handle(h) {}

    ~ScmHandle() {
        Close();
    }

    ScmHandle(const ScmHandle&) = delete;
    ScmHandle& operator=(const ScmHandle&) = delete;

    ScmHandle(ScmHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }

    ScmHandle& operator=(ScmHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    void Close() {
        if (m_handle) {
            CloseServiceHandle(m_handle);
            m_handle = nullptr;
        }
    }

    SC_HANDLE Get() const { return m_handle; }
    explicit operator bool() const { return m_handle != nullptr; }
};

class ServiceController {
public:
    static bool WaitForState(SC_HANDLE svc, DWORD wantedState, DWORD timeoutMs) {
        DWORD start = GetTickCount();
        SERVICE_STATUS_PROCESS st{};
        DWORD needed = 0;

        while (true) {
            if (!QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&st), sizeof(st), &needed)) {
                return false;
            }
            if (st.dwCurrentState == wantedState) {
                return true;
            }
            if (GetTickCount() - start > timeoutMs) {
                return false;
            }
            Sleep(250);
        }
    }
};

// ============================================================================
// 2. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

struct StartsrcOptions {
    std::wstring service;
    DWORD timeoutMs = 30000;
    bool help = false;
    bool version = false;
};

class OptionParser {
public:
    static void PrintHelp() {
        std::wcout << LR"(startsrc(1)             CrossShell for UNIX Reference Manual                 startsrc(1)

    NAME
        startsrc - start a Windows service subsystem

    SYNOPSIS
        startsrc -s SERVICE [-t SECONDS] [OPTIONS]

    DESCRIPTION
        Starts a Windows service using AIX System Resource Controller (SRC)
        syntax. Queries and monitors service state transitions via the Windows
        Service Control Manager (SCM) until running or the timeout expires.

    OPTIONS
        -s SERVICE
            Specify the Windows service name or short identifier to start.

        -t SECONDS
            Wait timeout in seconds for the service to start (default: 30).

        -h, --help
            Display this reference manual.

        -v, --version
            Display version information and exit.

    EXAMPLES
        startsrc -s wuauserv
            Start the Windows Update service with default 30-second timeout.

        startsrc -s Spooler -t 60
            Start the Print Spooler service with a 60-second timeout.

    CrossShell for UNIX                                                    startsrc(1)
    )";
    }

    static void PrintVersion() {
        std::cout << "startsrc v1.0.0\n";
    }

    bool Parse(int argc, wchar_t* argv[], StartsrcOptions& opt) const {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                opt.help = true;
                return true;
            }
            if (arg == L"-v" || arg == L"--version") {
                opt.version = true;
                return true;
            }
            if (arg == L"-s" && i + 1 < argc) {
                opt.service = argv[++i];
                continue;
            }
            if (arg == L"-t" && i + 1 < argc) {
                wchar_t* end = nullptr;
                unsigned long s = wcstoul(argv[++i], &end, 10);
                if (end == argv[i] || *end != L'\0' || s == 0) {
                    std::wcerr << L"startsrc: invalid timeout '" << argv[i] << L"'\n";
                    return false;
                }
                opt.timeoutMs = static_cast<DWORD>(s * 1000UL);
                continue;
            }
            std::wcerr << L"startsrc: unknown option '" << arg << L"'\n";
            return false;
        }
        return true;
    }
};

class StartsrcApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        StartsrcOptions opt;
        if (!m_parser.Parse(argc, argv, opt)) {
            std::cerr << "Try 'startsrc --help' for usage.\n";
            return 2;
        }

        if (opt.help) {
            OptionParser::PrintHelp();
            return 0;
        }

        if (opt.version) {
            OptionParser::PrintVersion();
            return 0;
        }

        if (opt.service.empty()) {
            std::cerr << "startsrc: missing required option -s SERVICE\n";
            return 2;
        }

        ScmHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
        if (!scm) {
            std::cerr << "startsrc: cannot open Service Control Manager (error " << GetLastError() << ")\n";
            return 1;
        }

        ScmHandle svc(OpenServiceW(scm.Get(), opt.service.c_str(), SERVICE_START | SERVICE_QUERY_STATUS));
        if (!svc) {
            DWORD err = GetLastError();
            std::wcerr << L"startsrc: cannot open service '" << opt.service << L"' (error " << err << L")\n";
            return 1;
        }

        SERVICE_STATUS_PROCESS st{};
        DWORD needed = 0;
        if (QueryServiceStatusEx(svc.Get(), SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&st), sizeof(st), &needed)) {
            if (st.dwCurrentState == SERVICE_RUNNING) {
                std::wcout << L"0513-059 The subsystem has already been started. Subsystem: " << opt.service << L"\n";
                return 0;
            }
        }

        if (!StartServiceW(svc.Get(), 0, nullptr)) {
            DWORD err = GetLastError();
            std::wcerr << L"startsrc: failed to start service '" << opt.service << L"' (error " << err << L")\n";
            return 1;
        }

        if (!ServiceController::WaitForState(svc.Get(), SERVICE_RUNNING, opt.timeoutMs)) {
            std::wcerr << L"startsrc: timed out waiting for service to run: " << opt.service << L"\n";
            return 1;
        }

        std::wcout << L"0513-059 The subsystem has been started. Subsystem: " << opt.service << L"\n";
        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    StartsrcApplication app;
    return app.Run(argc, argv);
}
