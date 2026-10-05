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
#include <winnt.h>
#include <winbase.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <cwchar>
#include <cstdint>
#include <memory>

// ============================================================================
// 1. DATA MODELS & RAII HANDLES
// ============================================================================

class ScopedJobHandle {
public:
    explicit ScopedJobHandle(HANDLE handle = NULL) : m_handle(handle) {}

    ~ScopedJobHandle() {
        Close();
    }

    ScopedJobHandle(const ScopedJobHandle&) = delete;
    ScopedJobHandle& operator=(const ScopedJobHandle&) = delete;

    ScopedJobHandle(ScopedJobHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = NULL;
    }

    ScopedJobHandle& operator=(ScopedJobHandle&& other) noexcept {
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

    void Reset(HANDLE handle = NULL) {
        Close();
        m_handle = handle;
    }

private:
    HANDLE m_handle;
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

struct ProcessTimeMetrics {
    double realSec = 0.0;
    double userSec = 0.0;
    double sysSec = 0.0;
    size_t peakRam = 0;
    size_t pageFaults = 0;
    size_t totalProcs = 1;
    uint64_t ioReadOps = 0;
    uint64_t ioWriteOps = 0;
    uint64_t ioReadBytes = 0;
    uint64_t ioWriteBytes = 0;
    DWORD exitCode = 0;
};

// ============================================================================
// 2. OPTIONS PARSER & STRING HELPERS
// ============================================================================

class TimeOptions {
public:
    bool posixFormat = false;
    bool detailedInfo = false;
    bool humanReadable = false;
    std::wstring outputFile;
    bool appendOutput = false;
    bool showHelp = false;
    bool showVersion = false;
    int cmdStart = -1;

    bool Parse(int argc, wchar_t* argv[]) {
        cmdStart = argc;

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--help") {
                showHelp = true;
                return true;
            } else if (arg == L"--version") {
                showVersion = true;
                return true;
            } else if (arg == L"--") {
                cmdStart = i + 1;
                break;
            } else if (arg == L"-p" || arg == L"--posix") {
                posixFormat = true;
            } else if (arg == L"-l" || arg == L"--detailed") {
                detailedInfo = true;
            } else if (arg == L"-h" || arg == L"--human") {
                humanReadable = true;
            } else if (arg == L"-a" || arg == L"--append") {
                appendOutput = true;
            } else if ((arg == L"-o" || arg == L"--output") && i + 1 < argc) {
                outputFile = argv[++i];
            } else if (arg.rfind(L"-", 0) == 0 && arg.length() > 1 && arg[1] != L'-') {
                bool valid = true;
                for (size_t j = 1; j < arg.length(); ++j) {
                    wchar_t c = arg[j];
                    if (c == L'p') posixFormat = true;
                    else if (c == L'l') detailedInfo = true;
                    else if (c == L'h') humanReadable = true;
                    else if (c == L'a') appendOutput = true;
                    else { valid = false; break; }
                }
                if (!valid) {
                    cmdStart = i;
                    break;
                }
            } else {
                cmdStart = i;
                break;
            }
        }

        return true;
    }

    void PrintHelp() const {
        std::wcout << LR"(ptime(1)                   CrossShell for UNIX Reference Manual                  ptime(1)

    NAME
        ptime - measure command execution time and resource utilization

    SYNOPSIS
        ptime [OPTIONS] [--] COMMAND [ARGS...]

    DESCRIPTION
        ptime executes the specified COMMAND with optional ARGS, measures its
        elapsed real time, user CPU time, and system CPU time, and writes
        timing and resource statistics to standard error (or to a specified
        file).

    OPTIONS
        -p, --posix
            Use POSIX-standard timing format (real, user, sys on separate lines).

        -l, --detailed
            Report detailed resource utilization statistics (memory, page faults,
            process counts, and I/O operations).

        -h, --human
            Format elapsed time in human-readable notation (e.g., 1m23.456s).

        -o, --output <file>
            Write timing statistics to <file> instead of standard error.

        -a, --append
            Append timing output to the file specified with -o.

        --
            End option processing. Subsequent arguments are treated as the command
            and its arguments even if they begin with a dash.

        --help
            Display this reference manual and exit.

        --version
            Display version information and exit.

    EXAMPLES
        ptime cmd /c dir
            Measure execution time of directory listing.

        ptime -p -l -- myapp.exe -v
            Time myapp.exe in POSIX format with detailed memory and I/O statistics.

        ptime -o timing.log -a -- build.bat
            Append build execution times to timing.log.

    CrossShell for UNIX                                                          ptime(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"ptime 1.0.0\n";
    }
};

// ============================================================================
// 3. EXECUTION TIMER ENGINE & REPORTER
// ============================================================================

class ProcessTimerEngine {
public:
    static std::wstring BuildCommandLine(const std::vector<std::wstring>& args) {
        std::wstring result;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) result += L" ";
            std::wstring arg = args[i];
            bool needs_quotes = arg.empty() || arg.find_first_of(L" \t\n\v\"") != std::wstring::npos;
            if (needs_quotes) {
                result += L"\"";
                for (wchar_t c : arg) {
                    if (c == L'"') result += L"\\\"";
                    else result += c;
                }
                result += L"\"";
            } else {
                result += arg;
            }
        }
        return result;
    }

    static int ExecuteAndMeasure(int argc, wchar_t* argv[], const TimeOptions& options, ProcessTimeMetrics& metrics) {
        if (options.cmdStart >= argc) {
            std::fwprintf(stderr, L"ptime: missing command operand\nTry 'ptime --help' for more information.\n");
            return 1;
        }

        std::vector<std::wstring> child_args;
        for (int i = options.cmdStart; i < argc; ++i) {
            child_args.push_back(argv[i]);
        }

        std::wstring cmd_line = BuildCommandLine(child_args);

        ScopedJobHandle hJob(CreateJobObjectW(NULL, NULL));

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};

        BOOL success = CreateProcessW(
            NULL,
            &cmd_line[0],
            NULL, NULL, TRUE,
            CREATE_SUSPENDED,
            NULL, NULL, &si, &pi
        );

        if (!success) {
            DWORD err = GetLastError();
            if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
                std::vector<std::wstring> cmd_wrapper = { L"cmd.exe", L"/c" };
                cmd_wrapper.insert(cmd_wrapper.end(), child_args.begin(), child_args.end());
                cmd_line = BuildCommandLine(cmd_wrapper);

                success = CreateProcessW(
                    NULL,
                    &cmd_line[0],
                    NULL, NULL, TRUE,
                    CREATE_SUSPENDED,
                    NULL, NULL, &si, &pi
                );
            }
        }

        if (!success) {
            DWORD err = GetLastError();
            std::fwprintf(stderr, L"time: cannot execute '%s': Win32 error %lu\n", child_args[0].c_str(), err);
            return 127;
        }

        if (hJob.IsValid()) {
            AssignProcessToJobObject(hJob.Get(), pi.hProcess);
        }

        LARGE_INTEGER qpc_freq, qpc_start, qpc_end;
        QueryPerformanceFrequency(&qpc_freq);
        QueryPerformanceCounter(&qpc_start);

        ResumeThread(pi.hThread);
        CloseHandle(pi.hThread);

        ScopedProcessHandle hProcess(pi.hProcess);
        WaitForSingleObject(hProcess.Get(), INFINITE);

        QueryPerformanceCounter(&qpc_end);

        GetExitCodeProcess(hProcess.Get(), &metrics.exitCode);

        metrics.realSec = static_cast<double>(qpc_end.QuadPart - qpc_start.QuadPart) / qpc_freq.QuadPart;

        if (hJob.IsValid()) {
            JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION basicIoInfo = {};
            if (QueryInformationJobObject(hJob.Get(), JobObjectBasicAndIoAccountingInformation,
                                          &basicIoInfo, sizeof(basicIoInfo), NULL)) {
                metrics.userSec = static_cast<double>(basicIoInfo.BasicInfo.TotalUserTime.QuadPart) / 10000000.0;
                metrics.sysSec  = static_cast<double>(basicIoInfo.BasicInfo.TotalKernelTime.QuadPart) / 10000000.0;
                metrics.pageFaults = basicIoInfo.BasicInfo.TotalPageFaultCount;
                metrics.totalProcs = basicIoInfo.BasicInfo.TotalProcesses;
                metrics.ioReadOps   = basicIoInfo.IoInfo.ReadOperationCount;
                metrics.ioWriteOps  = basicIoInfo.IoInfo.WriteOperationCount;
                metrics.ioReadBytes  = basicIoInfo.IoInfo.ReadTransferCount;
                metrics.ioWriteBytes = basicIoInfo.IoInfo.WriteTransferCount;
            }

            JOBOBJECT_EXTENDED_LIMIT_INFORMATION extLimitInfo = {};
            if (QueryInformationJobObject(hJob.Get(), JobObjectExtendedLimitInformation,
                                          &extLimitInfo, sizeof(extLimitInfo), NULL)) {
                metrics.peakRam = extLimitInfo.PeakJobMemoryUsed;
            }
        }

        return 0;
    }
};

class PtimeReporter {
public:
    static std::wstring FormatSeconds(double sec) {
        wchar_t buf[64];
        swprintf_s(buf, L"%.2f", sec);
        return buf;
    }

    static std::wstring FormatHuman(double sec) {
        int total_sec = static_cast<int>(sec);
        int minutes = total_sec / 60;
        double remaining_sec = sec - (minutes * 60);
        wchar_t buf[64];
        if (minutes > 0) {
            swprintf_s(buf, L"%dm%.3fs", minutes, remaining_sec);
        } else {
            swprintf_s(buf, L"%.3fs", remaining_sec);
        }
        return buf;
    }

    static void Report(const TimeOptions& opts, const ProcessTimeMetrics& metrics) {
        std::wstring out_str;
        if (opts.posixFormat) {
            if (opts.humanReadable) {
                out_str += L"real " + FormatHuman(metrics.realSec) + L"\n";
                out_str += L"user " + FormatHuman(metrics.userSec) + L"\n";
                out_str += L"sys  " + FormatHuman(metrics.sysSec) + L"\n";
            } else {
                out_str += L"real " + FormatSeconds(metrics.realSec) + L"\n";
                out_str += L"user " + FormatSeconds(metrics.userSec) + L"\n";
                out_str += L"sys  " + FormatSeconds(metrics.sysSec) + L"\n";
            }
        } else {
            if (opts.humanReadable) {
                out_str += L"\t" + FormatHuman(metrics.realSec) + L" real " +
                           L"\t" + FormatHuman(metrics.userSec) + L" user " +
                           L"\t" + FormatHuman(metrics.sysSec) + L" sys\n";
            } else {
                out_str += L"\t" + FormatSeconds(metrics.realSec) + L" real " +
                           L"\t" + FormatSeconds(metrics.userSec) + L" user " +
                           L"\t" + FormatSeconds(metrics.sysSec) + L" sys\n";
            }
        }

        if (opts.detailedInfo) {
            wchar_t details[512];
            swprintf_s(details,
                L"%12zu  maximum resident set size (bytes)\n"
                L"%12zu  page faults\n"
                L"%12zu  processes created\n"
                L"%12llu  I/O read operations\n"
                L"%12llu  I/O write operations\n"
                L"%12llu  I/O bytes read\n"
                L"%12llu  I/O bytes written\n",
                metrics.peakRam, metrics.pageFaults, metrics.totalProcs,
                metrics.ioReadOps, metrics.ioWriteOps, metrics.ioReadBytes, metrics.ioWriteBytes
            );
            out_str += details;
        }

        WriteOutput(opts, out_str);
    }

private:
    static void WriteOutput(const TimeOptions& opts, const std::wstring& text) {
        if (!opts.outputFile.empty()) {
            std::wofstream out(opts.outputFile, opts.appendOutput ? std::ios::app : std::ios::out);
            if (out.is_open()) {
                out << text;
                return;
            }
        }

        HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
        DWORD mode;
        if (GetConsoleMode(hErr, &mode)) {
            DWORD written;
            WriteConsoleW(hErr, text.c_str(), static_cast<DWORD>(text.length()), &written, NULL);
        } else {
            int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.length()), NULL, 0, NULL, NULL);
            if (size > 0) {
                std::string utf8_str(size, 0);
                WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.length()), &utf8_str[0], size, NULL, NULL);
                DWORD written;
                WriteFile(hErr, utf8_str.data(), static_cast<DWORD>(utf8_str.length()), &written, NULL);
            }
        }
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class PtimeApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        TimeOptions options;
        if (!options.Parse(argc, argv)) {
            return 1;
        }

        if (options.showHelp) {
            options.PrintHelp();
            return 0;
        }

        if (options.showVersion) {
            options.PrintVersion();
            return 0;
        }

        ProcessTimeMetrics metrics;
        int execResult = ProcessTimerEngine::ExecuteAndMeasure(argc, argv, options, metrics);
        if (execResult != 0) {
            return execResult;
        }

        PtimeReporter::Report(options, metrics);
        return static_cast<int>(metrics.exitCode);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    PtimeApplication app;
    return app.Run(argc, argv);
}
