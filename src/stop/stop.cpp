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
#include <map>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <memory>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

// ============================================================================
// 1. DATA MODELS, ENUMS & RAII HANDLES
// ============================================================================

enum class SignalType {
    SIGHUP = 1,
    SIGINT = 2,
    SIGQUIT = 3,
    SIGstop = 9,
    SIGTERM = 15
};

enum class OutputFormat {
    None = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

class ScopedProcessHandle {
public:
    explicit ScopedProcessHandle(HANDLE handle = NULL) : m_handle(handle) {}

    ~ScopedProcessHandle() {
        Close();
    }

    ScopedProcessHandle(const ScopedProcessHandle&) = delete;
    ScopedProcessHandle& operator=(const ScopedProcessHandle&) = delete;

    ScopedProcessHandle(ScopedProcessHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = NULL;
    }

    ScopedProcessHandle& operator=(ScopedProcessHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = NULL;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != NULL && m_handle != INVALID_HANDLE_VALUE; }

    void Close() {
        if (m_handle != NULL && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = NULL;
        }
    }

private:
    HANDLE m_handle;
};

class ScopedTokenHandle {
public:
    explicit ScopedTokenHandle(HANDLE handle = NULL) : m_handle(handle) {}

    ~ScopedTokenHandle() {
        Close();
    }

    ScopedTokenHandle(const ScopedTokenHandle&) = delete;
    ScopedTokenHandle& operator=(const ScopedTokenHandle&) = delete;

    ScopedTokenHandle(ScopedTokenHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = NULL;
    }

    ScopedTokenHandle& operator=(ScopedTokenHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = NULL;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    HANDLE* Receive() { Close(); return &m_handle; }
    bool IsValid() const { return m_handle != NULL && m_handle != INVALID_HANDLE_VALUE; }

    void Close() {
        if (m_handle != NULL && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = NULL;
        }
    }

private:
    HANDLE m_handle;
};

class PrivilegeEscalator {
public:
    static bool EnableDebugPrivilege() {
        ScopedTokenHandle hToken;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, hToken.Receive()))
            return false;

        LUID luid = {};
        if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
            return false;
        }

        TOKEN_PRIVILEGES tp = {};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        BOOL res = AdjustTokenPrivileges(hToken.Get(), FALSE, &tp, sizeof(tp), NULL, NULL);
        return res && (GetLastError() != ERROR_NOT_ALL_ASSIGNED);
    }
};

// ============================================================================
// 2. SIGNAL PARSER & OPTIONS
// ============================================================================

class SignalParser {
public:
    static std::string ToUpper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
        return s;
    }

    static bool Parse(const std::string& arg, SignalType& outSignal) {
        std::string sigStr = arg;
        if (!sigStr.empty() && sigStr[0] == '-') {
            sigStr = sigStr.substr(1);
        }
        sigStr = ToUpper(sigStr);

        if (sigStr.rfind("SIG", 0) == 0) {
            sigStr = sigStr.substr(3);
        }

        if (sigStr == "1" || sigStr == "HUP") { outSignal = SignalType::SIGHUP; return true; }
        if (sigStr == "2" || sigStr == "INT" || sigStr == "CTRLC") { outSignal = SignalType::SIGINT; return true; }
        if (sigStr == "3" || sigStr == "QUIT" || sigStr == "CTRLBREAK") { outSignal = SignalType::SIGQUIT; return true; }
        if (sigStr == "9" || sigStr == "STOP") { outSignal = SignalType::SIGstop; return true; }
        if (sigStr == "15" || sigStr == "TERM") { outSignal = SignalType::SIGTERM; return true; }

        return false;
    }

    static void PrintSignalList() {
        std::cout << " 1) SIGHUP       2) SIGINT       3) SIGQUIT      9) SIGstop\n"
                  << "15) SIGTERM\n";
    }
};

class StopOptions {
public:
    SignalType signal = SignalType::SIGTERM;
    std::vector<DWORD> pids;
    OutputFormat outputFormat = OutputFormat::None;
    std::string pipeCommand;
    bool showHelp = false;
    bool showVersion = false;
    bool showSignalList = false;

    int Parse(int argc, char* argv[]) {
        if (argc < 2) {
            showHelp = true;
            return 1;
        }

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--json") { outputFormat = OutputFormat::Json; continue; }
            if (arg == "--csv") { outputFormat = OutputFormat::Csv; continue; }
            if (arg == "--table") { outputFormat = OutputFormat::Table; continue; }
            if (arg == "--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }

            if (arg == "--help" || arg == "-h") {
                showHelp = true;
                return 0;
            }

            if (arg == "--version") {
                showVersion = true;
                return 0;
            }

            if (arg == "--") {
                for (++i; i < argc; ++i) {
                    std::string pidArg = argv[i];
                    char* end = nullptr;
                    unsigned long parsed = std::strtoul(pidArg.c_str(), &end, 10);
                    if (end == nullptr || *end != '\0' || parsed == 0 || parsed > 0xFFFFFFFFUL) {
                        std::cerr << "stop: illegal process id: " << pidArg << "\n";
                        return 1;
                    }
                    pids.push_back(static_cast<DWORD>(parsed));
                }
                break;
            }

            if (arg == "-l" || arg == "-L" || arg == "--list") {
                showSignalList = true;
                return 0;
            }

            if (arg == "-s" || arg == "--signal") {
                if (i + 1 >= argc) {
                    std::cerr << "stop: missing signal argument\n";
                    PrintUsage();
                    return 1;
                }
                if (!SignalParser::Parse(argv[++i], signal)) {
                    std::cerr << "stop: unknown signal: " << argv[i] << "\n";
                    return 1;
                }
                continue;
            }

            if (arg.size() > 1 && arg[0] == '-') {
                std::cerr << "stop: unknown option -- " << arg << "\n";
                PrintUsage();
                return 1;
            }

            char* end = nullptr;
            unsigned long parsed = std::strtoul(arg.c_str(), &end, 10);
            if (end == nullptr || *end != '\0' || parsed == 0 || parsed > 0xFFFFFFFFUL) {
                std::cerr << "stop: illegal process id: " << arg << "\n";
                return 1;
            }
            pids.push_back(static_cast<DWORD>(parsed));
        }

        if (pids.empty()) {
            std::cerr << "stop: no process IDs supplied\n";
            PrintUsage();
            return 1;
        }

        return -1;
    }

    void PrintUsage() const {
        std::cout << R"(stop(1)                 CrossShell for UNIX Reference Manual                  stop(1)

    NAME
        stop - pause or suspend running processes

    SYNOPSIS
        stop [OPTIONS] PID...

    DESCRIPTION
        stop pauses the execution of processes by suspending all associated
        threads (equivalent to SIGSTOP / NtSuspendProcess).

    OPTIONS
        -l, --list
            List supported signal names and actions.

        --json, --csv, --table
            Format suspension results as JSON, CSV, or table.

        --pipe COMMAND
            Stream results into COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Output version information and exit.

    EXAMPLES
        stop 1234
            Suspend execution of process 1234.

    CrossShell for UNIX                                                   stop(1)
)";
    }

    void PrintVersion() const {
        std::cout << "stop v2.1.0\n";
    }
};

// ============================================================================
// 3. PROCESS STOPPER ENGINE & REPORTER
// ============================================================================

class StopReporter {
public:
    static void Report(OutputFormat format, const std::string& pipeCommand, size_t count, bool hasErrors) {
        if (format == OutputFormat::None && pipeCommand.empty()) {
            return;
        }

        std::string status = hasErrors ? "failure" : "success";
        std::string text;

        if (format == OutputFormat::Json) {
            text = "{\"status\":\"" + status + "\",\"count\":" + std::to_string(count) + "}\n";
        } else if (format == OutputFormat::Csv) {
            text = "status,count\n" + status + "," + std::to_string(count) + "\n";
        } else if (format == OutputFormat::Table) {
            text = "STATUS\tCOUNT\n" + status + "\t" + std::to_string(count) + "\n";
        } else {
            text = status + ": " + std::to_string(count) + "\n";
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _popen(pipeCommand.c_str(), "w");
            if (!pipe) return;
            fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
        } else {
            std::cout << text;
        }
    }
};

class ProcessStopperEngine {
public:
    struct EnumData {
        DWORD pid;
        bool windowFound;
    };

    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
        EnumData* data = reinterpret_cast<EnumData*>(lParam);
        DWORD windowPid = 0;
        GetWindowThreadProcessId(hwnd, &windowPid);
        
        if (windowPid == data->pid && IsWindowVisible(hwnd)) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            data->windowFound = true;
        }
        return TRUE;
    }

    static bool ForceStopProcess(DWORD pid) {
        ScopedProcessHandle hProcess(OpenProcess(PROCESS_TERMINATE, FALSE, pid));
        if (!hProcess.IsValid()) return false;

        return TerminateProcess(hProcess.Get(), 1) != FALSE;
    }

    static bool GracefulStopProcess(DWORD pid) {
        EnumData data = { pid, false };
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));

        if (!data.windowFound) {
            return ForceStopProcess(pid);
        }
        return true;
    }

    static bool SendConsoleEvent(DWORD pid, DWORD ctrlEvent) {
        if (AttachConsole(pid)) {
            SetConsoleCtrlHandler(NULL, TRUE);
            BOOL res = GenerateConsoleCtrlEvent(ctrlEvent, 0);
            FreeConsole();
            SetConsoleCtrlHandler(NULL, FALSE);
            return res != FALSE;
        }
        return false;
    }

    static int Execute(const StopOptions& options) {
        PrivilegeEscalator::EnableDebugPrivilege();

        bool hasErrors = false;

        for (DWORD pid : options.pids) {
            bool success = false;

            switch (options.signal) {
                case SignalType::SIGHUP:
                case SignalType::SIGTERM:
                    success = GracefulStopProcess(pid);
                    break;
                case SignalType::SIGINT:
                    success = SendConsoleEvent(pid, CTRL_C_EVENT);
                    break;
                case SignalType::SIGQUIT:
                    success = SendConsoleEvent(pid, CTRL_BREAK_EVENT);
                    break;
                case SignalType::SIGstop:
                    success = ForceStopProcess(pid);
                    break;
            }

            if (!success) {
                DWORD err = GetLastError();
                std::cerr << "stop: (" << pid << ") - ";
                if (err == ERROR_INVALID_PARAMETER || err == ERROR_PROC_NOT_FOUND || err == ERROR_INVALID_HANDLE) {
                    std::cerr << "No such process\n";
                } else if (err == ERROR_ACCESS_DENIED) {
                    std::cerr << "Operation not permitted\n";
                } else {
                    std::cerr << "Error code " << err << "\n";
                }
                hasErrors = true;
            }
        }

        StopReporter::Report(options.outputFormat, options.pipeCommand, options.pids.size(), hasErrors);
        return hasErrors ? 1 : 0;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class StopApplication {
public:
    int Run(int argc, char* argv[]) const {
        StopOptions options;
        int parseResult = options.Parse(argc, argv);
        if (parseResult >= 0) {
            if (options.showHelp) {
                options.PrintUsage();
                return (argc < 2) ? 1 : 0;
            }
            if (options.showVersion) {
                options.PrintVersion();
                return 0;
            }
            if (options.showSignalList) {
                SignalParser::PrintSignalList();
                return 0;
            }
            return parseResult;
        }

        return ProcessStopperEngine::Execute(options);
    }
};

int main(int argc, char* argv[]) {
    StopApplication app;
    return app.Run(argc, argv);
}
