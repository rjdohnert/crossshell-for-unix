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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>

#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <cstdlib>
#include <cwctype>
#include <cstdio>
#include <streambuf>
#include <memory>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")

enum class OutputFormat {
    Text = 0,
    Json = 1,
    Csv = 2,
    Tsv = 3,
    Table = 4
};

enum class LimitMode {
    Both = 0,
    Soft = 1,
    Hard = 2
};

struct ProcessSnapshot {
    SIZE_T working_set = 0;
    SIZE_T peak_working_set = 0;
    SIZE_T private_usage = 0;
    ULONGLONG user_time_100ns = 0;
    ULONGLONG kernel_time_100ns = 0;
    DWORD handle_count = 0;
    DWORD thread_count = 0;
};

class ScopedJobHandle {
public:
    explicit ScopedJobHandle(HANDLE handle = NULL) : m_handle(handle) {}
    ~ScopedJobHandle() { Close(); }
    ScopedJobHandle(const ScopedJobHandle&) = delete;
    ScopedJobHandle& operator=(const ScopedJobHandle&) = delete;
    ScopedJobHandle(ScopedJobHandle&& other) noexcept : m_handle(other.m_handle) { other.m_handle = NULL; }
    ScopedJobHandle& operator=(ScopedJobHandle&& other) noexcept {
        if (this != &other) { Close(); m_handle = other.m_handle; other.m_handle = NULL; }
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

class ScopedProcessHandle {
public:
    explicit ScopedProcessHandle(HANDLE handle = NULL) : m_handle(handle) {}
    ~ScopedProcessHandle() { Close(); }
    ScopedProcessHandle(const ScopedProcessHandle&) = delete;
    ScopedProcessHandle& operator=(const ScopedProcessHandle&) = delete;
    ScopedProcessHandle(ScopedProcessHandle&& other) noexcept : m_handle(other.m_handle) { other.m_handle = NULL; }
    ScopedProcessHandle& operator=(ScopedProcessHandle&& other) noexcept {
        if (this != &other) { Close(); m_handle = other.m_handle; other.m_handle = NULL; }
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
    ~ScopedTokenHandle() { Close(); }
    ScopedTokenHandle(const ScopedTokenHandle&) = delete;
    ScopedTokenHandle& operator=(const ScopedTokenHandle&) = delete;
    ScopedTokenHandle(ScopedTokenHandle&& other) noexcept : m_handle(other.m_handle) { other.m_handle = NULL; }
    ScopedTokenHandle& operator=(ScopedTokenHandle&& other) noexcept {
        if (this != &other) { Close(); m_handle = other.m_handle; other.m_handle = NULL; }
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

class WidePipeBuffer : public std::wstreambuf {
public:
    explicit WidePipeBuffer(FILE* f) : file_(f) { setp(buffer_, buffer_ + 1024); }
    int_type overflow(int_type c) override {
        if (c != traits_type::eof()) { *pptr() = static_cast<wchar_t>(c); pbump(1); }
        return sync() == 0 ? traits_type::not_eof(c) : traits_type::eof();
    }
    int sync() override {
        auto n = pptr() - pbase();
        if (n) { fwrite(pbase(), sizeof(wchar_t), static_cast<size_t>(n), file_); }
        setp(buffer_, buffer_ + 1024);
        return fflush(file_);
    }
private:
    FILE* file_;
    wchar_t buffer_[1024];
};

class PrivilegeEscalator {
public:
    static bool EnableDebugPrivilege() {
        ScopedTokenHandle token;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, token.Receive())) return false;
        LUID luid;
        if (!LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid)) return false;
        TOKEN_PRIVILEGES privileges = {};
        privileges.PrivilegeCount = 1;
        privileges.Privileges[0].Luid = luid;
        privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if (!AdjustTokenPrivileges(token.Get(), FALSE, &privileges, sizeof(privileges), nullptr, nullptr)) return false;
        return GetLastError() != ERROR_NOT_ALL_ASSIGNED;
    }
};
// ============================================================================
// 2. FORMATTING HELPERS & OPTIONS PARSER
// ============================================================================

class FormatHelper {
public:
    static std::wstring FormatBytesIec(ULONGLONG bytes) {
        static const wchar_t* units[] = { L"B", L"KiB", L"MiB", L"GiB", L"TiB" };
        double value = static_cast<double>(bytes);
        size_t unit_index = 0;
        while (value >= 1024.0 && unit_index + 1 < _countof(units)) {
            value /= 1024.0;
            ++unit_index;
        }
        wchar_t buffer[64];
        if (unit_index == 0) swprintf_s(buffer, L"%llu %s", bytes, units[unit_index]);
        else swprintf_s(buffer, L"%.2f %s", value, units[unit_index]);
        return buffer;
    }

    static std::wstring FormatDurationSeconds(ULONGLONG hundred_ns) {
        double seconds = static_cast<double>(hundred_ns) / 10000000.0;
        wchar_t buffer[64];
        swprintf_s(buffer, L"%.3fs", seconds);
        return buffer;
    }

    static bool ParseUnsigned(const std::wstring& text, unsigned long long& value) {
        try {
            size_t consumed = 0;
            unsigned long long parsed = std::stoull(text, &consumed, 10);
            if (consumed != text.size()) return false;
            value = parsed;
            return true;
        } catch (...) {
            return false;
        }
    }

    static std::wstring QuoteArgument(const std::wstring& arg) {
        if (arg.empty()) return L"\"\"";
        bool needs_quotes = false;
        for (wchar_t ch : arg) {
            if (iswspace(ch) || ch == L'"') { needs_quotes = true; break; }
        }
        if (!needs_quotes) return arg;
        std::wstring quoted;
        quoted.push_back(L'"');
        size_t backslashes = 0;
        for (wchar_t ch : arg) {
            if (ch == L'\\') { ++backslashes; continue; }
            if (ch == L'"') { quoted.append(backslashes * 2 + 1, L'\\'); quoted.push_back(L'"'); backslashes = 0; continue; }
            if (backslashes > 0) { quoted.append(backslashes, L'\\'); backslashes = 0; }
            quoted.push_back(ch);
        }
        if (backslashes > 0) quoted.append(backslashes * 2, L'\\');
        quoted.push_back(L'"');
        return quoted;
    }

    static std::wstring BuildCommandLine(const std::vector<std::wstring>& args) {
        std::wstring command_line;
        for (size_t i = 0; i < args.size(); ++i) {
            if (!command_line.empty()) command_line.push_back(L' ');
            command_line += QuoteArgument(args[i]);
        }
        return command_line;
    }
};

class UlimitOptions {
public:
    bool show_help = false;
    bool show_version = false;
    bool show_all = false;
    bool parse_error = false;
    bool kill_on_close = false;
    LimitMode mode = LimitMode::Both;
    DWORD priority_class = 0;

    std::optional<unsigned long long> core_size_blocks;
    std::optional<unsigned long long> data_seg_kb;
    std::optional<unsigned long long> nice_priority;
    std::optional<unsigned long long> file_size_blocks;
    std::optional<unsigned long long> pending_signals;
    std::optional<unsigned long long> locked_mem_kb;
    std::optional<unsigned long long> memory_megabytes;
    std::optional<unsigned long long> open_files;
    std::optional<unsigned long long> pipe_size_blocks;
    std::optional<unsigned long long> msg_queue_bytes;
    std::optional<unsigned long long> rt_priority;
    std::optional<unsigned long long> stack_size_kb;
    std::optional<unsigned long long> cpu_seconds;
    std::optional<DWORD> process_count;
    std::optional<unsigned long long> virtual_mem_mb;
    std::optional<unsigned long long> file_locks;
    std::optional<unsigned long long> working_set_mb;

    std::vector<std::wstring> command;
    OutputFormat output_format = OutputFormat::Text;
    std::wstring pipe_command;

    void Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] != nullptr ? argv[i] : L"";
            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") { show_help = true; continue; }
            if (arg == L"-V" || arg == L"--version") { show_version = true; continue; }
            if (arg == L"-a" || arg == L"--all") { show_all = true; continue; }
            if (arg == L"-S" || arg == L"--soft") { mode = LimitMode::Soft; continue; }
            if (arg == L"-H" || arg == L"--hard") { mode = LimitMode::Hard; continue; }
            if (arg == L"--kill-on-close") { kill_on_close = true; continue; }
            if (arg == L"--json" || arg == L"-j") { output_format = OutputFormat::Json; continue; }
            if (arg == L"--csv") { output_format = OutputFormat::Csv; continue; }
            if (arg == L"--tsv") { output_format = OutputFormat::Tsv; continue; }
            if (arg == L"--table") { output_format = OutputFormat::Table; continue; }
            if (arg == L"--output" && i + 1 < argc) {
                std::wstring fmt = argv[++i];
                if (fmt == L"json") output_format = OutputFormat::Json;
                else if (fmt == L"csv") output_format = OutputFormat::Csv;
                else if (fmt == L"tsv") output_format = OutputFormat::Tsv;
                else if (fmt == L"table") output_format = OutputFormat::Table;
                continue;
            }
            if (arg == L"--pipe" && i + 1 < argc) { pipe_command = argv[++i]; continue; }

            if (arg == L"--priority" && i + 1 < argc) {
                std::wstring pri = argv[++i];
                if (pri == L"idle") priority_class = IDLE_PRIORITY_CLASS;
                else if (pri == L"below_normal" || pri == L"below-normal") priority_class = BELOW_NORMAL_PRIORITY_CLASS;
                else if (pri == L"normal") priority_class = NORMAL_PRIORITY_CLASS;
                else if (pri == L"above_normal" || pri == L"above-normal") priority_class = ABOVE_NORMAL_PRIORITY_CLASS;
                else if (pri == L"high") priority_class = HIGH_PRIORITY_CLASS;
                else if (pri == L"realtime") priority_class = REALTIME_PRIORITY_CLASS;
                continue;
            }

            auto parse_opt = [&](std::optional<unsigned long long>& opt_val, const wchar_t* opt_name) -> bool {
                if (i + 1 >= argc || (argv[i + 1][0] == L'-' && !std::iswdigit(argv[i + 1][1]))) {
                    opt_val = 0;
                    return true;
                }
                unsigned long long value = 0;
                if (!FormatHelper::ParseUnsigned(argv[++i], value)) {
                    std::wcerr << L"ulimit: invalid " << opt_name << L" value: " << argv[i] << L"\n";
                    parse_error = true;
                    return false;
                }
                opt_val = value;
                return true;
            };

            if (arg == L"-c" || arg == L"--core") { if (!parse_opt(core_size_blocks, L"core size")) return; continue; }
            if (arg == L"-d" || arg == L"--data") { if (!parse_opt(data_seg_kb, L"data segment")) return; continue; }
            if (arg == L"-e" || arg == L"--nice") { if (!parse_opt(nice_priority, L"nice priority")) return; continue; }
            if (arg == L"-f" || arg == L"--file-size") { if (!parse_opt(file_size_blocks, L"file size")) return; continue; }
            if (arg == L"-i" || arg == L"--signals") { if (!parse_opt(pending_signals, L"pending signals")) return; continue; }
            if (arg == L"-l" || arg == L"--locked-mem") { if (!parse_opt(locked_mem_kb, L"locked memory")) return; continue; }
            if (arg == L"-m" || arg == L"--memory") { if (!parse_opt(memory_megabytes, L"memory")) return; continue; }
            if (arg == L"-n" || arg == L"--open-files") { if (!parse_opt(open_files, L"open files")) return; continue; }
            if (arg == L"-p" || arg == L"--pipe-size") { if (!parse_opt(pipe_size_blocks, L"pipe size")) return; continue; }
            if (arg == L"-q" || arg == L"--posix-mq") { if (!parse_opt(msg_queue_bytes, L"message queue")) return; continue; }
            if (arg == L"-r" || arg == L"--realtime-priority") { if (!parse_opt(rt_priority, L"real-time priority")) return; continue; }
            if (arg == L"-s" || arg == L"--stack") { if (!parse_opt(stack_size_kb, L"stack size")) return; continue; }
            if (arg == L"-t" || arg == L"--cpu") { if (!parse_opt(cpu_seconds, L"CPU time")) return; continue; }
            if (arg == L"-u" || arg == L"--processes") {
                unsigned long long value = 0;
                if (i + 1 < argc && (argv[i + 1][0] != L'-' || std::iswdigit(argv[i + 1][1]))) {
                    if (!FormatHelper::ParseUnsigned(argv[++i], value) || value > 0xFFFFFFFFULL) {
                        std::wcerr << L"ulimit: invalid process count: " << argv[i] << L"\n";
                        parse_error = true;
                        return;
                    }
                    process_count = static_cast<DWORD>(value);
                } else {
                    process_count = 0;
                }
                continue;
            }
            if (arg == L"-v" || arg == L"--virtual-memory") { if (!parse_opt(virtual_mem_mb, L"virtual memory")) return; continue; }
            if (arg == L"-w" || arg == L"--working-set") { if (!parse_opt(working_set_mb, L"working set")) return; continue; }
            if (arg == L"-x" || arg == L"--file-locks") { if (!parse_opt(file_locks, L"file locks")) return; continue; }

            if (arg == L"--") {
                for (++i; i < argc; ++i) command.push_back(argv[i] != nullptr ? argv[i] : L"");
                break;
            }

            if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"ulimit: unknown option -- " << arg << L"\nTry 'ulimit --help' for more information.\n";
                parse_error = true;
                return;
            }

            for (; i < argc; ++i) command.push_back(argv[i] != nullptr ? argv[i] : L"");
            break;
        }
    }
};
// ============================================================================
// 3. PROCESS USAGE INSPECTOR & JOB LIMIT MANAGER
// ============================================================================

class ProcessUsageInspector {
public:
    static bool SnapshotProcess(HANDLE process_handle, ProcessSnapshot& snapshot) {
        PROCESS_MEMORY_COUNTERS_EX counters = {};
        counters.cb = sizeof(counters);
        if (!GetProcessMemoryInfo(process_handle, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
            return false;
        }

        FILETIME creation_time = {}, exit_time = {}, kernel_time = {}, user_time = {};
        if (!GetProcessTimes(process_handle, &creation_time, &exit_time, &kernel_time, &user_time)) {
            return false;
        }

        DWORD handle_cnt = 0;
        GetProcessHandleCount(process_handle, &handle_cnt);

        snapshot.working_set = counters.WorkingSetSize;
        snapshot.peak_working_set = counters.PeakWorkingSetSize;
        snapshot.private_usage = static_cast<SIZE_T>(counters.PrivateUsage);
        snapshot.kernel_time_100ns = (static_cast<ULONGLONG>(kernel_time.dwHighDateTime) << 32) | kernel_time.dwLowDateTime;
        snapshot.user_time_100ns = (static_cast<ULONGLONG>(user_time.dwHighDateTime) << 32) | user_time.dwLowDateTime;
        snapshot.handle_count = handle_cnt;
        return true;
    }
};

class JobLimitManager {
public:
    static bool SetJobLimits(HANDLE job_handle, const UlimitOptions& options) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
        info.BasicLimitInformation.LimitFlags = 0;

        if (options.kill_on_close) {
            info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        }

        if (options.priority_class != 0) {
            info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PRIORITY_CLASS;
            info.BasicLimitInformation.PriorityClass = options.priority_class;
        }

        if (options.memory_megabytes.has_value() && options.memory_megabytes.value() > 0) {
            info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
            info.ProcessMemoryLimit = static_cast<SIZE_T>(options.memory_megabytes.value() * 1024ULL * 1024ULL);
        }

        if (options.virtual_mem_mb.has_value() && options.virtual_mem_mb.value() > 0) {
            info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_JOB_MEMORY;
            info.JobMemoryLimit = static_cast<SIZE_T>(options.virtual_mem_mb.value() * 1024ULL * 1024ULL);
        }

        if (options.working_set_mb.has_value() && options.working_set_mb.value() > 0) {
            info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_WORKINGSET;
            info.BasicLimitInformation.MinimumWorkingSetSize = static_cast<SIZE_T>(1024 * 1024);
            info.BasicLimitInformation.MaximumWorkingSetSize = static_cast<SIZE_T>(options.working_set_mb.value() * 1024ULL * 1024ULL);
        }

        if (options.cpu_seconds.has_value() && options.cpu_seconds.value() > 0) {
            info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_TIME;
            info.BasicLimitInformation.PerProcessUserTimeLimit.QuadPart = static_cast<LONGLONG>(options.cpu_seconds.value() * 10000000ULL);
        }

        if (options.process_count.has_value() && options.process_count.value() > 0) {
            info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
            info.BasicLimitInformation.ActiveProcessLimit = options.process_count.value();
        }

        if (info.BasicLimitInformation.LimitFlags == 0) {
            return true;
        }

        return SetInformationJobObject(job_handle, JobObjectExtendedLimitInformation, &info, sizeof(info)) != FALSE;
    }

    static bool RunChildUnderLimits(const UlimitOptions& options) {
        if (options.command.empty()) return true;

        std::wstring command_line = FormatHelper::BuildCommandLine(options.command);
        std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
        mutable_command.push_back(L'\0');

        STARTUPINFOW startup = {};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process_info = {};

        ScopedJobHandle job_handle(CreateJobObjectW(nullptr, nullptr));
        if (!job_handle.IsValid()) {
            std::wcerr << L"ulimit: unable to create job object\n";
            return false;
        }

        if (!SetJobLimits(job_handle.Get(), options)) {
            std::wcerr << L"ulimit: unable to apply job limits\n";
            return false;
        }

        DWORD creation_flags = CREATE_SUSPENDED;
        if (options.priority_class != 0) {
            creation_flags |= options.priority_class;
        }

        if (!CreateProcessW(nullptr, mutable_command.data(), nullptr, nullptr, FALSE,
                            creation_flags, nullptr, nullptr, &startup, &process_info)) {
            std::wcerr << L"ulimit: unable to start command: " << command_line << L"\n";
            return false;
        }

        ScopedProcessHandle hProcess(process_info.hProcess);
        ScopedProcessHandle hThread(process_info.hThread);

        if (!AssignProcessToJobObject(job_handle.Get(), hProcess.Get())) {
            std::wcerr << L"ulimit: unable to assign command to job object\n";
            TerminateProcess(hProcess.Get(), 1);
            return false;
        }

        ResumeThread(hThread.Get());
        WaitForSingleObject(hProcess.Get(), INFINITE);

        DWORD exit_code = 1;
        GetExitCodeProcess(hProcess.Get(), &exit_code);

        return exit_code == 0;
    }
};

// ============================================================================
// 4. REPORTER & HELP SYSTEM
// ============================================================================

class UlimitReporter {
public:
    static void PrintSummary(OutputFormat format) {
        ProcessSnapshot snapshot = {};
        if (!ProcessUsageInspector::SnapshotProcess(GetCurrentProcess(), snapshot)) {
            std::wcerr << L"ulimit: unable to query current process resource usage\n";
            return;
        }

        if (format == OutputFormat::Json) {
            std::wcout << L"{\"working_set\":" << snapshot.working_set 
                       << L",\"peak_working_set\":" << snapshot.peak_working_set 
                       << L",\"private_usage\":" << snapshot.private_usage 
                       << L",\"user_time_100ns\":" << snapshot.user_time_100ns 
                       << L",\"kernel_time_100ns\":" << snapshot.kernel_time_100ns
                       << L",\"open_handles\":" << snapshot.handle_count << L"}\n";
            return;
        }
        if (format == OutputFormat::Csv) {
            std::wcout << L"metric,value,formatted\n" 
                       << L"working_set," << snapshot.working_set << L"," << FormatHelper::FormatBytesIec(snapshot.working_set) << L"\n"
                       << L"peak_working_set," << snapshot.peak_working_set << L"," << FormatHelper::FormatBytesIec(snapshot.peak_working_set) << L"\n"
                       << L"private_usage," << snapshot.private_usage << L"," << FormatHelper::FormatBytesIec(snapshot.private_usage) << L"\n"
                       << L"user_time_100ns," << snapshot.user_time_100ns << L"," << FormatHelper::FormatDurationSeconds(snapshot.user_time_100ns) << L"\n"
                       << L"kernel_time_100ns," << snapshot.kernel_time_100ns << L"," << FormatHelper::FormatDurationSeconds(snapshot.kernel_time_100ns) << L"\n"
                       << L"open_handles," << snapshot.handle_count << L"," << snapshot.handle_count << L"\n";
            return;
        }
        if (format == OutputFormat::Tsv) {
            std::wcout << L"METRIC\tVALUE\tFORMATTED\n" 
                       << L"working_set\t" << snapshot.working_set << L"\t" << FormatHelper::FormatBytesIec(snapshot.working_set) << L"\n"
                       << L"peak_working_set\t" << snapshot.peak_working_set << L"\t" << FormatHelper::FormatBytesIec(snapshot.peak_working_set) << L"\n"
                       << L"private_usage\t" << snapshot.private_usage << L"\t" << FormatHelper::FormatBytesIec(snapshot.private_usage) << L"\n"
                       << L"user_time_100ns\t" << snapshot.user_time_100ns << L"\t" << FormatHelper::FormatDurationSeconds(snapshot.user_time_100ns) << L"\n"
                       << L"kernel_time_100ns\t" << snapshot.kernel_time_100ns << L"\t" << FormatHelper::FormatDurationSeconds(snapshot.kernel_time_100ns) << L"\n"
                       << L"open_handles\t" << snapshot.handle_count << L"\t" << snapshot.handle_count << L"\n";
            return;
        }
        if (format == OutputFormat::Table) {
            std::wcout << L"RESOURCE                       CURRENT VALUE          LIMIT\n"
                       << L"-----------------------------------------------------------\n"
                       << L"core file size (blocks, -c)    unlimited              unlimited\n"
                       << L"data seg size (kbytes, -d)     unlimited              unlimited\n"
                       << L"scheduling priority (-e)       0                      0\n"
                       << L"file size (blocks, -f)         unlimited              unlimited\n"
                       << L"pending signals (-i)           unlimited              unlimited\n"
                       << L"max locked memory (kbytes, -l) unlimited              unlimited\n"
                       << L"max memory size (MB, -m)       " << FormatHelper::FormatBytesIec(snapshot.private_usage) << L"              unlimited\n"
                       << L"open files / handles (-n)      " << snapshot.handle_count << L"                    unlimited\n"
                       << L"pipe size (512 bytes, -p)      8                      8\n"
                       << L"POSIX message queues (-q)      unlimited              unlimited\n"
                       << L"real-time priority (-r)        0                      0\n"
                       << L"stack size (kbytes, -s)        1024                   unlimited\n"
                       << L"cpu time (seconds, -t)         " << FormatHelper::FormatDurationSeconds(snapshot.user_time_100ns) << L"               unlimited\n"
                       << L"max user processes (-u)        unlimited              unlimited\n"
                       << L"virtual memory (MB, -v)        " << FormatHelper::FormatBytesIec(snapshot.working_set) << L"              unlimited\n"
                       << L"file locks (-x)                unlimited              unlimited\n";
            return;
        }

        std::wcout << L"core file size          (blocks, -c) unlimited\n"
                   << L"data seg size           (kbytes, -d) unlimited\n"
                   << L"scheduling priority             (-e) 0\n"
                   << L"file size               (blocks, -f) unlimited\n"
                   << L"pending signals                 (-i) unlimited\n"
                   << L"max locked memory       (kbytes, -l) unlimited\n"
                   << L"max memory size                 (MB, -m) " << FormatHelper::FormatBytesIec(snapshot.private_usage) << L"\n"
                   << L"open files                      (-n) " << snapshot.handle_count << L"\n"
                   << L"pipe size            (512 bytes, -p) 8\n"
                   << L"POSIX message queues     (bytes, -q) unlimited\n"
                   << L"real-time priority              (-r) 0\n"
                   << L"stack size              (kbytes, -s) 1024\n"
                   << L"cpu time               (seconds, -t) " << FormatHelper::FormatDurationSeconds(snapshot.user_time_100ns) << L"\n"
                   << L"max user processes              (-u) unlimited\n"
                   << L"virtual memory                  (MB, -v) " << FormatHelper::FormatBytesIec(snapshot.working_set) << L"\n"
                   << L"file locks                      (-x) unlimited\n";
    }

    static void PrintHelp() {
        std::wcout << LR"(ulimit(1)               CrossShell for UNIX Reference Manual                 ulimit(1)

    NAME
        ulimit - get and set process and job resource limits

    SYNOPSIS
        ulimit [-SHacdflmnpqrstuvx] [LIMIT]
        ulimit [QUALIFIERS] [--] [COMMAND [ARGUMENTS...]]
        ulimit [OPTIONS]

    DESCRIPTION
        ulimit provides control over the resources available to the current
        shell, child processes, or command execution contexts. On Windows,
        resource constraints are enforced via Windows NT Job Objects and
        Process Token Security Privilege management.

    RESOURCE LIMIT QUALIFIERS
        -a, --all
            Report all current process resource usages and supported limits.

        -c [LIMIT]
            Core file size limit (Windows Crash Dump limit).

        -d [LIMIT]
            Maximum data segment size (kbytes).

        -e [LIMIT]
            Maximum scheduling priority (nice level).

        -f [LIMIT]
            Maximum file size written (blocks).

        -l [LIMIT]
            Maximum locked-in-memory address space (kbytes).

        -m, --memory [LIMIT_MB]
            Maximum physical memory / working set size (MB).

        -n [LIMIT]
            Maximum number of open file handles / descriptors.

        -p [LIMIT]
            Pipe buffer size in 512-byte blocks.

        -s [LIMIT]
            Maximum stack size (kbytes).

        -t, --cpu [SECONDS]
            Maximum CPU user time allowed in seconds.

        -u, --processes [N]
            Maximum number of active processes in the job.

        -v, --virtual-memory [LIMIT_MB]
            Maximum virtual memory / pagefile commit limit (MB).

        -w, --working-set [LIMIT_MB]
            Maximum working set size (MB).

        -x [LIMIT]
            Maximum number of file locks.

        -S, --soft
            Set or query the soft resource limit.

        -H, --hard
            Set or query the hard resource limit.

    WINDOWS JOB CONTROLS
        --priority LEVEL
            Set process priority class: idle, below_normal, normal,
            above_normal, high, or realtime.

        --kill-on-close
            Ensure all child processes terminate when the job handle closes.

    OUTPUT & INTEGRATION OPTIONS
        --output FORMAT
            Select table, csv, tsv, or json output. The default is table.

        --json, -j, --csv, --tsv, --table
            Convenience shortcuts for structured output formats.

        --pipe COMMAND
            Stream formatted output directly to another command or utility.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

        --
            Delimit options from command and arguments to execute.

    EXAMPLES
        ulimit -a
            Display current resource usage snapshot and limits.

        ulimit -a --json
            Display resource snapshot formatted as JSON.

        ulimit -m 512 -- myapp.exe
            Run myapp.exe constrained to 512 MB memory.

        ulimit -t 30 -m 1024 -- cmd.exe /c heavy_task.cmd
            Execute task with a 30-second CPU limit and 1 GB memory limit.

        ulimit -u 4 -- make -j8
            Restrict process tree to at most 4 active concurrent processes.

    CrossShell for UNIX                                                     ulimit(1)
)";
    }

    static void PrintVersion() {
        std::wcout << L"ulimit (CrossShell) 5.0.0\n"
                   << L"Copyright (c) 2026 Roberto J Dohnert. All rights reserved.\n";
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER & MAIN ENTRY
// ============================================================================

class UlimitApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        UlimitOptions options;
        options.Parse(argc, argv);

        if (options.show_version) {
            UlimitReporter::PrintVersion();
            return 0;
        }

        if (options.show_help) {
            UlimitReporter::PrintHelp();
            return 0;
        }

        if (options.parse_error) {
            return 1;
        }

        FILE* outputPipe = nullptr;
        WidePipeBuffer* pipeBuffer = nullptr;
        std::wstreambuf* oldOutput = nullptr;
        if (!options.pipe_command.empty()) {
            outputPipe = _wpopen(options.pipe_command.c_str(), L"w");
            if (!outputPipe) return 1;
            oldOutput = std::wcout.rdbuf();
            pipeBuffer = new WidePipeBuffer(outputPipe);
            std::wcout.rdbuf(pipeBuffer);
        }

        if (options.show_all || options.command.empty()) {
            UlimitReporter::PrintSummary(options.output_format);
            if (options.command.empty()) {
                if (pipeBuffer) {
                    std::wcout.flush();
                    std::wcout.rdbuf(oldOutput);
                    delete pipeBuffer;
                    _pclose(outputPipe);
                }
                return 0;
            }
        }

        PrivilegeEscalator::EnableDebugPrivilege();

        if (!options.command.empty()) {
            int result = JobLimitManager::RunChildUnderLimits(options) ? 0 : 1;
            if (pipeBuffer) {
                std::wcout.flush();
                std::wcout.rdbuf(oldOutput);
                delete pipeBuffer;
                _pclose(outputPipe);
            }
            return result;
        }

        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    UlimitApplication app;
    return app.Run(argc, argv);
}