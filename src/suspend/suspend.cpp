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

#include <windows.h>
#include <tlhelp32.h>
#pragma comment(lib, "advapi32.lib")

#include <iostream>
#include <string>
#include <vector>
#include <cwctype>
#include <cstdio>

using NtSuspendProcessFn = LONG (NTAPI*)(HANDLE);
using NtResumeProcessFn = LONG (NTAPI*)(HANDLE);

// ============================================================================
// 1. PRIVILEGE MANAGER & STRING UTILS
// ============================================================================

class PrivilegeManager {
public:
    static bool EnableDebugPrivilege() {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
            return false;
        }

        LUID luid;
        if (!LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid)) {
            CloseHandle(token);
            return false;
        }

        TOKEN_PRIVILEGES privileges = {};
        privileges.PrivilegeCount = 1;
        privileges.Privileges[0].Luid = luid;
        privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        if (!AdjustTokenPrivileges(token, FALSE, &privileges, sizeof(privileges), nullptr, nullptr)) {
            CloseHandle(token);
            return false;
        }

        const bool ok = GetLastError() != ERROR_NOT_ALL_ASSIGNED;
        CloseHandle(token);
        return ok;
    }
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

    static std::wstring ToLowerCopy(const std::wstring& value) {
        std::wstring lower;
        lower.reserve(value.size());
        for (wchar_t ch : value) {
            lower.push_back(static_cast<wchar_t>(towlower(ch)));
        }
        return lower;
    }

    static bool ParsePid(const std::wstring& text, DWORD& pid) {
        try {
            size_t consumed = 0;
            unsigned long parsed = std::stoul(text, &consumed, 10);
            if (consumed != text.size() || parsed == 0 || parsed > 0xFFFFFFFFUL) {
                return false;
            }
            pid = static_cast<DWORD>(parsed);
            return true;
        } catch (...) {
            return false;
        }
    }
};

// ============================================================================
// 2. PROCESS SUSPEND / RESUME ENGINE
// ============================================================================

class ProcessSuspendEngine {
private:
    static bool OperateWithNtdll(HANDLE process_handle, bool resume) {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (ntdll == nullptr) {
            return false;
        }

        auto suspend_fn = reinterpret_cast<NtSuspendProcessFn>(GetProcAddress(ntdll, "NtSuspendProcess"));
        auto resume_fn = reinterpret_cast<NtResumeProcessFn>(GetProcAddress(ntdll, "NtResumeProcess"));
        if (!resume && suspend_fn == nullptr) {
            return false;
        }
        if (resume && resume_fn == nullptr) {
            return false;
        }

        LONG status = resume ? resume_fn(process_handle) : suspend_fn(process_handle);
        return status >= 0;
    }

    static bool OperateWithThreads(DWORD pid, bool resume) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            return false;
        }

        THREADENTRY32 entry = {};
        entry.dwSize = sizeof(entry);
        bool touched = false;

        if (Thread32First(snapshot, &entry)) {
            do {
                if (entry.th32OwnerProcessID != pid) {
                    continue;
                }

                HANDLE thread_handle = OpenThread(THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID);
                if (thread_handle == nullptr) {
                    continue;
                }

                if (resume) {
                    while (ResumeThread(thread_handle) > 0) {
                    }
                } else {
                    SuspendThread(thread_handle);
                }

                touched = true;
                CloseHandle(thread_handle);
            } while (Thread32Next(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return touched;
    }

public:
    static bool OperateOnPid(DWORD pid, bool resume, bool use_ntdll) {
        if (pid == GetCurrentProcessId()) {
            std::wcerr << L"suspend: refusing to target the current process\n";
            return false;
        }

        HANDLE process_handle = OpenProcess(PROCESS_SUSPEND_RESUME | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (process_handle != nullptr) {
            bool ok = false;
            if (use_ntdll) {
                ok = OperateWithNtdll(process_handle, resume);
            }
            CloseHandle(process_handle);
            if (ok) {
                return true;
            }
        }

        return OperateWithThreads(pid, resume);
    }
};

// ============================================================================
// 3. OUTPUT FORMATTER
// ============================================================================

class OutputFormatter {
public:
    static void Emit(int format, const std::wstring& pipeCommand, bool resume, bool all_ok) {
        if (format == 0 && pipeCommand.empty()) return;

        std::wstring text;
        if (format == 1) {
            text = L"{\"action\":\"" + std::wstring(resume ? L"resume" : L"suspend") + L"\",\"success\":" + std::wstring(all_ok ? L"true" : L"false") + L"}\n";
        } else if (format == 2) {
            text = L"action,success\n" + std::wstring(resume ? L"resume," : L"suspend,") + (all_ok ? L"true\n" : L"false\n");
        } else {
            text = L"ACTION\tSUCCESS\n" + std::wstring(resume ? L"resume\t" : L"suspend\t") + (all_ok ? L"true\n" : L"false\n");
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _wpopen(pipeCommand.c_str(), L"w");
            if (pipe) {
                std::string narrow = StringUtils::Utf8(text);
                fwrite(narrow.data(), 1, narrow.size(), pipe);
                _pclose(pipe);
            }
        } else {
            std::wcout << text;
        }
    }
};

// ============================================================================
// 4. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

struct SuspendOptions {
    bool resume = false;
    bool show_help = false;
    bool show_version = false;
    bool parse_error = false;
    bool safe_mode = false;
    bool enable_debug_priv = false;
    bool use_ntdll = true;
    std::vector<DWORD> pids;
    int output_format = 0;
    std::wstring pipe_command;
};

class OptionParser {
public:
    static void PrintUsage(const wchar_t* program_name) {
        std::wcout << L"Usage: " << program_name << L" [options] PID...\n"
                   << L"Suspend or resume one or more processes.\n\n"
                   << L"Options:\n"
                   << L"  -r, --resume   Resume the target processes instead of suspending them\n"
                   << L"      --safe     Force conservative mode (no debug privilege, no ntdll fast path)\n"
                   << L"  -d, --debug    Enable SeDebugPrivilege (off by default)\n"
                   << L"      --no-ntdll Avoid NtSuspendProcess/NtResumeProcess fast path\n"
                   << L"  -h, --help     Display this help text\n"
                   << L"      --json     Output operation status as JSON\n"
                   << L"      --csv      Output operation status as CSV\n"
                   << L"      --table    Output operation status as a table\n"
                   << L"      --pipe COMMAND  Send output through COMMAND\n"
                   << L"      --version  Display version information\n\n"
                   << L"Examples:\n"
                   << L"  suspend 1234\n"
                   << L"  suspend -r 1234\n"
                   << L"  suspend --debug 1234\n"
                   << L"  suspend --safe 1234\n";
    }

    static void PrintVersion() {
        std::wcout << L"suspend v1.0.0\n";
    }

    SuspendOptions Parse(int argc, wchar_t* argv[]) const {
        SuspendOptions options;

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] != nullptr ? argv[i] : L"";

            if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                options.show_help = true;
                continue;
            }

            if (arg == L"--version") {
                options.show_version = true;
                continue;
            }

            if (arg == L"-r" || arg == L"--resume") {
                options.resume = true;
                continue;
            }

            if (arg == L"--safe") {
                options.safe_mode = true;
                continue;
            }
            if (arg == L"--json") { options.output_format = 1; continue; }
            if (arg == L"--csv") { options.output_format = 2; continue; }
            if (arg == L"--table") { options.output_format = 3; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { options.pipe_command = argv[++i]; continue; }

            if (arg == L"-d" || arg == L"--debug") {
                options.enable_debug_priv = true;
                continue;
            }

            if (arg == L"--no-ntdll") {
                options.use_ntdll = false;
                continue;
            }

            if (arg == L"--") {
                for (++i; i < argc; ++i) {
                    DWORD pid = 0;
                    if (!StringUtils::ParsePid(argv[i], pid)) {
                        std::wcerr << L"suspend: illegal process id: " << argv[i] << L"\n";
                        options.parse_error = true;
                        return options;
                    }
                    options.pids.push_back(pid);
                }
                break;
            }

            if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"suspend: unknown option -- " << arg << L"\n";
                options.parse_error = true;
                return options;
            }

            DWORD pid = 0;
            if (!StringUtils::ParsePid(arg, pid)) {
                std::wcerr << L"suspend: illegal process id: " << arg << L"\n";
                options.parse_error = true;
                return options;
            }
            options.pids.push_back(pid);
        }

        if (options.safe_mode) {
            options.enable_debug_priv = false;
            options.use_ntdll = false;
        }

        return options;
    }
};

class SuspendApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) const {
        if (argc > 0 && argv[0] != nullptr && (argc == 1 || std::wstring(argv[0]).empty())) {
            OptionParser::PrintUsage(L"suspend");
            return 1;
        }

        SuspendOptions options = m_parser.Parse(argc, argv);

        if (options.show_version) {
            OptionParser::PrintVersion();
            return 0;
        }

        if (options.show_help) {
            const wchar_t* program_name = (argc > 0 && argv[0] != nullptr) ? argv[0] : L"suspend";
            OptionParser::PrintUsage(program_name);
            return 0;
        }

        if (options.parse_error) {
            return 1;
        }

        if (options.pids.empty()) {
            OptionParser::PrintUsage((argc > 0 && argv[0] != nullptr) ? argv[0] : L"suspend");
            return 1;
        }

        if (options.enable_debug_priv && !PrivilegeManager::EnableDebugPrivilege()) {
            std::wcerr << L"suspend: warning: failed to enable SeDebugPrivilege; continuing without it\n";
        }

        bool all_ok = true;
        for (DWORD pid : options.pids) {
            if (!ProcessSuspendEngine::OperateOnPid(pid, options.resume, options.use_ntdll)) {
                std::wcerr << L"suspend: failed to " << (options.resume ? L"resume" : L"suspend") << L" process " << pid << L"\n";
                all_ok = false;
            }
        }

        OutputFormatter::Emit(options.output_format, options.pipe_command, options.resume, all_ok);
        return all_ok ? 0 : 1;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    SuspendApplication app;
    return app.Run(argc, argv);
}
