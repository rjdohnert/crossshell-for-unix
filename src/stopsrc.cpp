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
#include <cstdio>

#pragma comment(lib, "advapi32.lib")

// ============================================================================
// 1. RAII SERVICE HANDLE & STRING UTILITIES
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

class StringUtils {
public:
    static std::string Utf8(const std::wstring& value) {
        int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
        return result;
    }
};

enum class OutputFormat {
    Default = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

class OutputFormatter {
private:
    OutputFormat m_format{OutputFormat::Default};
    std::wstring m_pipeCommand;

public:
    OutputFormatter(OutputFormat fmt, std::wstring pipeCmd)
        : m_format(fmt), m_pipeCommand(std::move(pipeCmd)) {}

    void Emit(const std::wstring& message) const {
        std::wstring text;
        switch (m_format) {
            case OutputFormat::Json:
                text = L"{\"status\":\"success\",\"message\":\"" + message + L"\"}\n";
                break;
            case OutputFormat::Csv:
                text = L"status,message\nsuccess,\"" + message + L"\"\n";
                break;
            case OutputFormat::Table:
                text = L"STATUS\tMESSAGE\nsuccess\t" + message + L"\n";
                break;
            default:
                text = message + L"\n";
                break;
        }

        if (!m_pipeCommand.empty()) {
            FILE* p = _wpopen(m_pipeCommand.c_str(), L"w");
            if (p) {
                std::string n = StringUtils::Utf8(text);
                fwrite(n.data(), 1, n.size(), p);
                _pclose(p);
            }
        } else {
            std::wcout << text;
        }
    }
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

struct StopsrcOptions {
    std::wstring service;
    DWORD timeoutMs = 30000;
    bool help = false;
    bool version = false;
    OutputFormat format = OutputFormat::Default;
    std::wstring pipe;
};

class OptionParser {
public:
    static void PrintHelp() {
        std::wcout << LR"(stopsrc(1)              CrossShell for UNIX Reference Manual                  stopsrc(1)

    NAME
        stopsrc - stop a Windows service subsystem

    SYNOPSIS
        stopsrc -s SERVICE [-t SECONDS] [OPTIONS]

    DESCRIPTION
        Stops a running Windows service using AIX System Resource Controller (SRC)
        syntax. Requests service stop and monitors state transitions via the Windows
        Service Control Manager (SCM) until stopped or the timeout expires.

    OPTIONS
        -s SERVICE
            Specify the Windows service name or short identifier to stop.

        -t SECONDS
            Wait timeout in seconds for the service to stop (default: 30).

        --json
            Emit result telemetry in JSON format.

        --csv
            Emit result telemetry in CSV format.

        --table
            Emit result telemetry in tabular format.

        --pipe COMMAND
            Stream status directly into COMMAND.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version information and exit.

    EXAMPLES
        stopsrc -s wuauserv
            Stop the Windows Update service with default 30-second timeout.

        stopsrc -s Spooler -t 60
            Stop the Print Spooler service with a 60-second timeout.

    CrossShell for UNIX                                                     stopsrc(1)
    )";
    }

    static void PrintVersion() {
        std::cout << "stopsrc v1.0.0\n";
    }

    bool Parse(int argc, wchar_t* argv[], StopsrcOptions& opt) const {
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
            if (arg == L"--json") { opt.format = OutputFormat::Json; continue; }
            if (arg == L"--csv") { opt.format = OutputFormat::Csv; continue; }
            if (arg == L"--table") { opt.format = OutputFormat::Table; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { opt.pipe = argv[++i]; continue; }
            if (arg == L"-s" && i + 1 < argc) {
                opt.service = argv[++i];
                continue;
            }
            if (arg == L"-t" && i + 1 < argc) {
                wchar_t* end = nullptr;
                unsigned long s = wcstoul(argv[++i], &end, 10);
                if (end == argv[i] || *end != L'\0' || s == 0) {
                    std::wcerr << L"stopsrc: invalid timeout '" << argv[i] << L"'\n";
                    return false;
                }
                opt.timeoutMs = static_cast<DWORD>(s * 1000UL);
                continue;
            }
            std::wcerr << L"stopsrc: unknown option '" << arg << L"'\n";
            return false;
        }
        return true;
    }
};

class StopsrcApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        StopsrcOptions opt;
        if (!m_parser.Parse(argc, argv, opt)) {
            std::cerr << "Try 'stopsrc --help' for usage.\n";
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
            std::cerr << "stopsrc: missing required option -s SERVICE\n";
            return 2;
        }

        OutputFormatter formatter(opt.format, opt.pipe);

        ScmHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
        if (!scm) {
            std::cerr << "stopsrc: cannot open Service Control Manager (error " << GetLastError() << ")\n";
            return 1;
        }

        ScmHandle svc(OpenServiceW(scm.Get(), opt.service.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS));
        if (!svc) {
            DWORD err = GetLastError();
            std::wcerr << L"stopsrc: cannot open service '" << opt.service << L"' (error " << err << L")\n";
            return 1;
        }

        SERVICE_STATUS_PROCESS st{};
        DWORD needed = 0;
        if (QueryServiceStatusEx(svc.Get(), SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&st), sizeof(st), &needed)) {
            if (st.dwCurrentState == SERVICE_STOPPED) {
                formatter.Emit(L"0513-044 The subsystem was requested to stop. Subsystem: " + opt.service);
                return 0;
            }
        }

        SERVICE_STATUS svcStatus{};
        if (!ControlService(svc.Get(), SERVICE_CONTROL_STOP, &svcStatus)) {
            DWORD err = GetLastError();
            std::wcerr << L"stopsrc: failed to stop service '" << opt.service << L"' (error " << err << L")\n";
            return 1;
        }

        if (!ServiceController::WaitForState(svc.Get(), SERVICE_STOPPED, opt.timeoutMs)) {
            std::wcerr << L"stopsrc: timed out waiting for service to stop: " << opt.service << L"\n";
            return 1;
        }

        formatter.Emit(L"0513-044 The subsystem was requested to stop. Subsystem: " + opt.service);
        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    StopsrcApplication app;
    return app.Run(argc, argv);
}
