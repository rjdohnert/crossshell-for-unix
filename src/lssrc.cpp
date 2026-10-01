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
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <memory>

#pragma comment(lib, "advapi32.lib")

// ============================================================================
// 1. DATA MODELS & RAII HANDLERS
// ============================================================================

class ScopedServiceHandle {
public:
    explicit ScopedServiceHandle(SC_HANDLE handle = nullptr) : m_handle(handle) {}

    ~ScopedServiceHandle() {
        Close();
    }

    ScopedServiceHandle(const ScopedServiceHandle&) = delete;
    ScopedServiceHandle& operator=(const ScopedServiceHandle&) = delete;

    ScopedServiceHandle(ScopedServiceHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }

    ScopedServiceHandle& operator=(ScopedServiceHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    SC_HANDLE Get() const { return m_handle; }
    operator SC_HANDLE() const { return m_handle; }
    bool IsValid() const { return m_handle != nullptr; }

    void Close() {
        if (m_handle) {
            CloseServiceHandle(m_handle);
            m_handle = nullptr;
        }
    }

    void Reset(SC_HANDLE handle = nullptr) {
        Close();
        m_handle = handle;
    }

private:
    SC_HANDLE m_handle;
};

struct ServiceRow {
    std::string name;
    DWORD pid = 0;
    std::string status;
};

// ============================================================================
// 2. SERVICE CONTROL MANAGER ENGINE
// ============================================================================

class ServiceManagerEngine {
public:
    static std::wstring ToLower(std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(towlower(ch));
        });
        return s;
    }

    static std::string ToUtf8(const std::wstring& w) {
        if (w.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return "";
        std::string out(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &out[0], size, nullptr, nullptr);
        return out;
    }

    static const char* StateToWord(DWORD state) {
        switch (state) {
            case SERVICE_RUNNING: return "active";
            case SERVICE_STOPPED: return "inoperative";
            case SERVICE_START_PENDING: return "starting";
            case SERVICE_STOP_PENDING: return "stopping";
            case SERVICE_PAUSED: return "paused";
            default: return "unknown";
        }
    }

    static bool EnumerateServices(bool includeStopped, std::vector<ServiceRow>& outRows, std::string& outError) {
        ScopedServiceHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE));
        if (!scm.IsValid()) {
            outError = "cannot open Service Control Manager (error " + std::to_string(GetLastError()) + ")";
            return false;
        }

        DWORD needed = 0;
        DWORD returned = 0;
        DWORD resume = 0;
        DWORD serviceType = SERVICE_WIN32;
        DWORD serviceState = includeStopped ? SERVICE_STATE_ALL : SERVICE_ACTIVE;

        EnumServicesStatusExW(
            scm.Get(),
            SC_ENUM_PROCESS_INFO,
            serviceType,
            serviceState,
            nullptr,
            0,
            &needed,
            &returned,
            &resume,
            nullptr
        );

        if (GetLastError() != ERROR_MORE_DATA || needed == 0) {
            outError = "failed to enumerate services (error " + std::to_string(GetLastError()) + ")";
            return false;
        }

        std::vector<BYTE> buffer(needed);
        if (!EnumServicesStatusExW(
                scm.Get(),
                SC_ENUM_PROCESS_INFO,
                serviceType,
                serviceState,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &needed,
                &returned,
                &resume,
                nullptr)) {
            outError = "failed to enumerate services (error " + std::to_string(GetLastError()) + ")";
            return false;
        }

        ENUM_SERVICE_STATUS_PROCESSW* entries = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*>(buffer.data());
        for (DWORD i = 0; i < returned; ++i) {
            std::wstring name = entries[i].lpServiceName ? entries[i].lpServiceName : L"";
            DWORD pid = entries[i].ServiceStatusProcess.dwProcessId;
            ServiceRow row{ ToUtf8(name), pid, StateToWord(entries[i].ServiceStatusProcess.dwCurrentState) };
            outRows.push_back(row);
        }

        return true;
    }
};

// ============================================================================
// 3. OPTIONS & COMMAND LINE PARSER
// ============================================================================

enum class LssrcFormat { Table, Csv, Json };

class LssrcOptions {
public:
    bool includeStopped = false;
    bool showHelp = false;
    bool showVersion = false;
    std::wstring specificService;
    LssrcFormat format = LssrcFormat::Table;
    std::vector<std::wstring> filters;

    bool Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"-a") {
                includeStopped = true;
                continue;
            }
            if (arg == L"-h" || arg == L"--help") {
                showHelp = true;
                return true;
            }
            if (arg == L"-v" || arg == L"-V" || arg == L"--version") {
                showVersion = true;
                return true;
            }
            if (arg == L"-s" && i + 1 < argc) {
                specificService = argv[++i] ? argv[i] : L"";
                continue;
            }
            if (arg == L"--table") { format = LssrcFormat::Table; continue; }
            if (arg == L"--csv") { format = LssrcFormat::Csv; continue; }
            if (arg == L"--json") { format = LssrcFormat::Json; continue; }
            if (arg == L"-") {
                std::wstring token;
                while (std::wcin >> token) filters.push_back(token);
                continue;
            }
            if (!arg.empty() && arg[0] != L'-') {
                filters.push_back(arg);
                continue;
            }
            std::wcerr << L"lssrc: unknown option '" << arg << L"'\n";
            return false;
        }
        return true;
    }

    void PrintUsage(const char* /*prog*/ = "lssrc") const {
        std::cout << R"(lssrc(1)                 CrossShell for UNIX Reference Manual                 lssrc(1)

    NAME
        lssrc - list Windows service status

    SYNOPSIS
        lssrc [OPTIONS] [FILTER...]

    DESCRIPTION
        Lists Windows services through the Service Control Manager. By default,
        active services are shown; filters match service names case-insensitively.

    OPTIONS
        -a
            Include stopped services.

        -s NAME
            Select exactly one service by service name.

        --table
            Use aligned table output (default).

        --csv
            Emit CSV output.

        --json
            Emit a JSON array.

        -
            Read service filters from standard input.

        -h, --help
            Display this comprehensive reference manual and exit.

        -v, -V, --version
            Display version information and exit.

    OUTPUT
        Table columns are Subsystem, PID, and Status. CSV fields are Service, PID,
        and Status; JSON objects contain service, pid, and status.

    EXAMPLES
        lssrc
            List active services in table format.

        lssrc -a --json
            Include stopped services and emit JSON.

        lssrc -s Spooler
            Show the exact status of the Print Spooler service.

        echo update | lssrc - --csv
            Read a service-name filter from standard input.

    EXIT STATUS
        0
            Help, version, or successful enumeration.
        1
            Invalid input, service-manager failure, or missing service.

    CrossShell for UNIX                                                    lssrc(1)
)";
    }

    void PrintVersion() const {
        std::cout << "lssrc 1.0.0\n";
    }
};

// ============================================================================
// 4. OUTPUT REPORTER
// ============================================================================

class LssrcReporter {
public:
    static std::string CsvEscape(const std::string& value) {
        std::string out = "\"";
        for (char c : value) out += (c == '"' ? "\"\"" : std::string(1, c));
        return out + '"';
    }

    static std::string JsonEscape(const std::string& value) {
        std::string out;
        for (char c : value) {
            if (c == '"' || c == '\\') out += '\\';
            if (c == '\n') out += "\\n";
            else out += c;
        }
        return out;
    }

    static void Emit(const LssrcOptions& opts, const std::vector<ServiceRow>& rows) {
        if (opts.format == LssrcFormat::Csv) {
            std::cout << "Service,PID,Status\n";
            for (const auto& row : rows) {
                std::cout << CsvEscape(row.name) << ',' << row.pid << ',' << CsvEscape(row.status) << '\n';
            }
            return;
        }

        if (opts.format == LssrcFormat::Json) {
            std::cout << "[\n";
            for (size_t i = 0; i < rows.size(); ++i) {
                const auto& row = rows[i];
                std::cout << "  {\"service\":\"" << JsonEscape(row.name) << "\",\"pid\":" << row.pid
                          << ",\"status\":\"" << JsonEscape(row.status) << "\"}"
                          << (i + 1 == rows.size() ? "\n" : ",\n");
            }
            std::cout << "]\n";
            return;
        }

        std::cout << std::left << std::setw(36) << "Subsystem" << std::setw(8) << "PID" << "Status\n";
        for (const auto& row : rows) {
            std::cout << std::setw(36) << row.name << std::setw(8) << row.pid << row.status << '\n';
        }
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class LssrcApplication {
public:
    int Run(int argc, wchar_t* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        LssrcOptions opts;
        if (!opts.Parse(argc, argv)) {
            std::cerr << "Try 'lssrc --help' for usage.\n";
            return 2;
        }

        if (opts.showHelp) {
            opts.PrintUsage("lssrc");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        std::vector<ServiceRow> allRows;
        std::string errorMsg;
        if (!ServiceManagerEngine::EnumerateServices(opts.includeStopped, allRows, errorMsg)) {
            std::cerr << "lssrc: " << errorMsg << "\n";
            return 1;
        }

        std::wstring match = ServiceManagerEngine::ToLower(opts.specificService);
        std::vector<ServiceRow> filteredRows;

        for (const auto& row : allRows) {
            std::wstring wName(row.name.begin(), row.name.end());
            if (!match.empty() && ServiceManagerEngine::ToLower(wName) != match) {
                continue;
            }

            bool selected = opts.filters.empty();
            std::string lowerName = row.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });

            for (const auto& filter : opts.filters) {
                std::string lowerFilter = ServiceManagerEngine::ToUtf8(ServiceManagerEngine::ToLower(filter));
                if (lowerName.find(lowerFilter) != std::string::npos) {
                    selected = true;
                    break;
                }
            }

            if (selected) {
                filteredRows.push_back(row);
            }
        }

        if (!match.empty() && filteredRows.empty()) {
            std::wcerr << L"lssrc: service not found: " << opts.specificService << L"\n";
            return 1;
        }

        LssrcReporter::Emit(opts, filteredRows);
        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    LssrcApplication app;
    return app.Run(argc, argv);
}

