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
#include <memory>

#pragma comment(lib, "advapi32.lib")

// ============================================================================
// 1. RAII HANDLES & RESOURCE GUARDS
// ============================================================================

class ScopedServiceHandle {
public:
    explicit ScopedServiceHandle(SC_HANDLE handle = NULL) : m_handle(handle) {}

    ~ScopedServiceHandle() {
        Close();
    }

    ScopedServiceHandle(const ScopedServiceHandle&) = delete;
    ScopedServiceHandle& operator=(const ScopedServiceHandle&) = delete;

    ScopedServiceHandle(ScopedServiceHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = NULL;
    }

    ScopedServiceHandle& operator=(ScopedServiceHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = NULL;
        }
        return *this;
    }

    SC_HANDLE Get() const { return m_handle; }
    SC_HANDLE* Receive() { Close(); return &m_handle; }
    bool IsValid() const { return m_handle != NULL; }
    operator SC_HANDLE() const { return m_handle; }

    void Close() {
        if (m_handle != NULL) {
            CloseServiceHandle(m_handle);
            m_handle = NULL;
        }
    }

    void Reset(SC_HANDLE handle = NULL) {
        Close();
        m_handle = handle;
    }

private:
    SC_HANDLE m_handle;
};

// ============================================================================
// 2. OPTIONS PARSER
// ============================================================================

class RefreshOptions {
public:
    std::wstring service;
    DWORD timeoutMs = 30000;
    bool restartFallback = false;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"-h" || arg == L"--help") {
                showHelp = true;
                return true;
            }
            if (arg == L"-v" || arg == L"--version") {
                showVersion = true;
                return true;
            }
            if (arg == L"-s" && i + 1 < argc) {
                service = argv[++i];
                continue;
            }
            if (arg == L"-r" || arg == L"--restart") {
                restartFallback = true;
                continue;
            }
            if (arg == L"-t" && i + 1 < argc) {
                wchar_t* end = nullptr;
                unsigned long s = wcstoul(argv[++i], &end, 10);
                if (end == argv[i] || *end != L'\0' || s == 0) {
                    std::wcerr << L"refresh: invalid timeout '" << argv[i] << L"'\n";
                    return false;
                }
                timeoutMs = static_cast<DWORD>(s * 1000UL);
                continue;
            }
            std::wcerr << L"refresh: unknown option '" << arg << L"'\n";
            return false;
        }
        return true;
    }

    void PrintHelp() const {
        std::cout << R"(refresh(1)                CrossShell for UNIX Reference Manual                refresh(1)

    NAME
        refresh - refresh a Windows service configuration

    SYNOPSIS
        refresh -s SERVICE [OPTIONS]

    DESCRIPTION
        refresh sends the SERVICE_CONTROL_PARAMCHANGE control signal to a
        Windows service to reload its configuration dynamically. With -r,
        it falls back to stopping and restarting the service if parameter
        change is not supported.

    OPTIONS
        -s <service>
            Specify the name of the service to refresh (required).

        -r, --restart
            Restart the service if parameter-change control is unsupported.

        -t <seconds>
            Set the operation timeout in seconds (default: 30).

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        refresh -s Spooler
            Refresh the Print Spooler service configuration.

        refresh -s W32Time -r
            Refresh time service or restart if param-change is unsupported.

        refresh -s MyService -t 60
            Refresh MyService with a 60-second wait timeout.

    CrossShell for UNIX                                                          refresh(1)
)";
    }

    void PrintVersion() const {
        std::cout << "refresh v1.0.0\n";
    }
};

// ============================================================================
// 3. SERVICE SUBSYSTEM MANAGER
// ============================================================================

class ServiceSubsystemManager {
public:
    static bool WaitForState(SC_HANDLE svc, DWORD wantedState, DWORD timeoutMs) {
        DWORD start = GetTickCount();
        SERVICE_STATUS_PROCESS st = {};
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

    static bool StopThenStart(SC_HANDLE svc, DWORD timeoutMs, const std::wstring& name) {
        SERVICE_STATUS status = {};
        if (!ControlService(svc, SERVICE_CONTROL_STOP, &status)) {
            DWORD err = GetLastError();
            if (err != ERROR_SERVICE_NOT_ACTIVE) {
                std::wcerr << L"refresh: stop failed for '" << name << L"' (error " << err << L")\n";
                return false;
            }
        }

        if (!WaitForState(svc, SERVICE_STOPPED, timeoutMs)) {
            std::wcerr << L"refresh: timed out waiting for stop: " << name << L"\n";
            return false;
        }

        if (!StartServiceW(svc, 0, nullptr)) {
            std::wcerr << L"refresh: start failed for '" << name << L"' (error " << GetLastError() << L")\n";
            return false;
        }

        if (!WaitForState(svc, SERVICE_RUNNING, timeoutMs)) {
            std::wcerr << L"refresh: timed out waiting for running state: " << name << L"\n";
            return false;
        }

        return true;
    }

    static int RefreshService(const RefreshOptions& opt) {
        if (opt.service.empty()) {
            std::cerr << "refresh: missing required option -s SERVICE\n";
            return 2;
        }

        ScopedServiceHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
        if (!scm.IsValid()) {
            std::cerr << "refresh: cannot open Service Control Manager (error " << GetLastError() << ")\n";
            return 1;
        }

        ScopedServiceHandle svc(OpenServiceW(
            scm.Get(),
            opt.service.c_str(),
            SERVICE_QUERY_STATUS | SERVICE_USER_DEFINED_CONTROL | SERVICE_START | SERVICE_STOP
        ));

        if (!svc.IsValid()) {
            DWORD err = GetLastError();
            std::wcerr << L"refresh: cannot open service '" << opt.service << L"' (error " << err << L")\n";
            return 1;
        }

        SERVICE_STATUS status = {};
        if (ControlService(svc.Get(), SERVICE_CONTROL_PARAMCHANGE, &status)) {
            std::wcout << L"0513-095 The subsystem has been refreshed. Subsystem: " << opt.service << L"\n";
            return 0;
        }

        DWORD err = GetLastError();
        if (opt.restartFallback &&
            (err == ERROR_INVALID_SERVICE_CONTROL || err == ERROR_CALL_NOT_IMPLEMENTED || err == ERROR_INVALID_FUNCTION)) {
            if (StopThenStart(svc.Get(), opt.timeoutMs, opt.service)) {
                std::wcout << L"0513-095 The subsystem has been refreshed by restart fallback. Subsystem: " << opt.service << L"\n";
                return 0;
            }
            return 1;
        }

        std::wcerr << L"refresh: service did not accept refresh control for '" << opt.service << L"' (error " << err << L")\n";
        if (!opt.restartFallback) {
            std::wcerr << L"refresh: rerun with --restart to allow restart fallback.\n";
        }

        return 1;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class RefreshApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        SetConsoleOutputCP(CP_UTF8);

        RefreshOptions options;
        if (!options.Parse(argc, argv)) {
            std::cerr << "Try 'refresh --help' for usage.\n";
            return 2;
        }

        if (options.showHelp) {
            options.PrintHelp();
            return 0;
        }

        if (options.showVersion) {
            options.PrintVersion();
            return 0;
        }

        return ServiceSubsystemManager::RefreshService(options);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    RefreshApplication app;
    return app.Run(argc, argv);
}

