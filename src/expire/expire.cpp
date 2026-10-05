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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#pragma comment(lib, "User32.lib")
#include <iostream>
#include <string>
#include <vector>
#include <cstdio>

static int g_expire_format = 0;
static std::wstring g_expire_pipe;

static std::string expire_to_utf8(const std::wstring& text) {
    if (text.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <map>

//
// CONSTANTS & SIGNAL DEFINITIONS
// 

constexpr DWORD EXIT_TIMEOUT_EXPIRED  = 124;
constexpr DWORD EXIT_INTERNAL_ERROR   = 125;
constexpr DWORD EXIT_CANNOT_INVOKE    = 126;
constexpr DWORD EXIT_CMD_NOT_FOUND    = 127;
constexpr DWORD EXIT_FORCE_KILLED     = 137; // 128 + 9 (SIGKILL)

enum UnixSignal {
    SIG_HUP  = 1,
    SIG_INT  = 2,
    SIG_QUIT = 3,
    SIG_KILL = 9,
    SIG_TERM = 15
};

// 
// RAII HANDLE WRAPPER
// 

class ScopedHandle {
    HANDLE h_ = INVALID_HANDLE_VALUE;
public:
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) : h_(h) {}
    ~ScopedHandle() { close(); }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept : h_(other.h_) { other.h_ = INVALID_HANDLE_VALUE; }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) { close(); h_ = other.h_; other.h_ = INVALID_HANDLE_VALUE; }
        return *this;
    }
    [[nodiscard]] HANDLE get() const { return h_; }
    void close() {
        if (h_ != INVALID_HANDLE_VALUE && h_ != NULL) {
            CloseHandle(h_);
            h_ = INVALID_HANDLE_VALUE;
        }
    }
    [[nodiscard]] bool is_valid() const { return h_ != INVALID_HANDLE_VALUE && h_ != NULL; }
};

// 
// WIN32 ARGUMENT ESCAPING & PATH RESOLUTION
// 

std::wstring escape_win32_arg(const std::wstring& arg) {
    if (arg.empty()) return L"\"\"";
    bool needs_quotes = false;
    for (wchar_t c : arg) {
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\v' || c == L'"') {
            needs_quotes = true;
            break;
        }
    }
    if (!needs_quotes) return arg;

    std::wstring out = L"\"";
    size_t backslashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') {
            backslashes++;
        } else if (c == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'"');
            backslashes = 0;
        } else {
            out.append(backslashes, L'\\');
            backslashes = 0;
            out.push_back(c);
        }
    }
    out.append(backslashes * 2, L'\\');
    out.push_back(L'"');
    return out;
}

std::wstring to_lower_w(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    return s;
}

std::wstring resolve_executable(const std::wstring& cmd) {
    wchar_t pathext_buf[32767];
    DWORD pathext_len = GetEnvironmentVariableW(L"PATHEXT", pathext_buf, 32767);
    std::vector<std::wstring> extensions = { L".exe", L".cmd", L".bat", L".com" };
    if (pathext_len > 0) {
        std::wstring pathext_wstr(pathext_buf, pathext_len);
        std::transform(pathext_wstr.begin(), pathext_wstr.end(), pathext_wstr.begin(), ::towlower);
        std::wstringstream ss(pathext_wstr);
        std::wstring ext;
        extensions.clear();
        while (std::getline(ss, ext, L';')) {
            if (!ext.empty()) extensions.push_back(ext);
        }
    }

    auto try_extensions = [&](const std::wstring& base) -> std::wstring {
        DWORD attr = GetFileAttributesW(base.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) return base;
        for (const auto& ext : extensions) {
            std::wstring candidate = base + ext;
            DWORD cattr = GetFileAttributesW(candidate.c_str());
            if (cattr != INVALID_FILE_ATTRIBUTES && !(cattr & FILE_ATTRIBUTE_DIRECTORY)) {
                return candidate;
            }
        }
        return L"";
    };

    if (cmd.find(L'\\') != std::wstring::npos || cmd.find(L'/') != std::wstring::npos) {
        std::wstring res = try_extensions(cmd);
        if (!res.empty()) return res;
    }

    wchar_t buf[MAX_PATH];
    for (const auto& ext : extensions) {
        DWORD len = SearchPathW(NULL, cmd.c_str(), ext.c_str(), MAX_PATH, buf, NULL);
        if (len > 0 && len < MAX_PATH) return std::wstring(buf, len);
    }

    DWORD len = SearchPathW(NULL, cmd.c_str(), NULL, MAX_PATH, buf, NULL);
    if (len > 0 && len < MAX_PATH) return std::wstring(buf, len);

    return L"";
}

bool is_batch_file(const std::wstring& path) {
    std::wstring ext = to_lower_w(path);
    return (ext.length() >= 4 && ext.substr(ext.length() - 4) == L".bat") ||
           (ext.length() >= 4 && ext.substr(ext.length() - 4) == L".cmd");
}

// ============================================================================
// DURATION & SIGNAL PARSING
// ============================================================================

bool parse_duration(const std::wstring& str, double& seconds_out) {
    if (str.empty()) return false;
    std::wstring num_part = str;
    wchar_t suffix = L's';

    wchar_t last_c = ::towlower(str.back());
    if (last_c == L's' || last_c == L'm' || last_c == L'h' || last_c == L'd') {
        suffix = last_c;
        num_part = str.substr(0, str.length() - 1);
    }

    try {
        size_t idx = 0;
        double val = std::stod(num_part, &idx);
        if (idx != num_part.length() || val < 0.0) return false;

        switch (suffix) {
            case L's': seconds_out = val; break;
            case L'm': seconds_out = val * 60.0; break;
            case L'h': seconds_out = val * 3600.0; break;
            case L'd': seconds_out = val * 86400.0; break;
            default: return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_signal(const std::wstring& str, DWORD& signal_out) {
    std::wstring s = to_lower_w(str);
    if (s.rfind(L"sig", 0) == 0) s = s.substr(3);

    static const std::map<std::wstring, DWORD> sig_map = {
        { L"hup", SIG_HUP },   { L"1", SIG_HUP },
        { L"int", SIG_INT },   { L"2", SIG_INT },
        { L"quit", SIG_QUIT }, { L"3", SIG_QUIT },
        { L"kill", SIG_KILL }, { L"9", SIG_KILL },
        { L"term", SIG_TERM }, { L"15", SIG_TERM }
    };

    auto it = sig_map.find(s);
    if (it != sig_map.end()) {
        signal_out = it->second;
        return true;
    }

    try {
        size_t idx = 0;
        DWORD val = std::stoul(str, &idx);
        if (idx == str.length()) {
            signal_out = val;
            return true;
        }
    } catch (...) {}

    return false;
}

std::wstring get_signal_name(DWORD sig) {
    switch (sig) {
        case SIG_HUP:  return L"SIGHUP";
        case SIG_INT:  return L"SIGINT";
        case SIG_QUIT: return L"SIGQUIT";
        case SIG_KILL: return L"SIGKILL";
        case SIG_TERM: return L"SIGTERM";
        default:       return L"SIG" + std::to_wstring(sig);
    }
}

// 
// PROCESS TREE SNAPSHOT FALLBACK (For environments denying Job Object nested assignment)
// 

void terminate_process_tree_snapshot(DWORD root_pid, UINT exit_code) {
    ScopedHandle hSnap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!hSnap.is_valid()) return;

    PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
    std::vector<DWORD> pids_to_kill;

    if (Process32FirstW(hSnap.get(), &pe)) {
        do {
            if (pe.th32ParentProcessID == root_pid) {
                pids_to_kill.push_back(pe.th32ProcessID);
            }
        } while (Process32NextW(hSnap.get(), &pe));
    }

    // Recursively kill child process trees first
    for (DWORD child_pid : pids_to_kill) {
        terminate_process_tree_snapshot(child_pid, exit_code);
    }

    // Terminate root PID
    ScopedHandle hProc(OpenProcess(PROCESS_TERMINATE, FALSE, root_pid));
    if (hProc.is_valid()) {
        TerminateProcess(hProc.get(), exit_code);
    }
}

//
// SIGNAL EMULATION & PROCESS TERMINATION
// 

BOOL CALLBACK kill_window_enum_proc(HWND hWnd, LPARAM lParam) {
    DWORD target_pid = static_cast<DWORD>(lParam);
    DWORD win_pid = 0;
    GetWindowThreadProcessId(hWnd, &win_pid);
    if (win_pid == target_pid && IsWindowVisible(hWnd)) {
        PostMessageW(hWnd, WM_CLOSE, 0, 0);
    }
    return TRUE;
}

void dispatch_win32_signal(HANDLE hJob, HANDLE hProcess, DWORD pid, DWORD signal, bool verbose) {
    if (verbose) {
        std::wcerr << L"expire: sending signal " << get_signal_name(signal)
                   << L" (" << signal << L") to command PID " << pid << L"\n";
    }

    if (signal == SIG_KILL) {
        if (hJob != INVALID_HANDLE_VALUE && hJob != NULL) {
            TerminateJobObject(hJob, EXIT_FORCE_KILLED);
        } else {
            terminate_process_tree_snapshot(pid, EXIT_FORCE_KILLED);
        }
        return;
    }

    if (signal == SIG_INT || signal == SIG_QUIT) {
        // CTRL_BREAK_EVENT reliably reaches process groups created with CREATE_NEW_PROCESS_GROUP
        GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid);
        return;
    }

    if (signal == SIG_TERM || signal == SIG_HUP) {
        // Attempt graceful window close for GUI processes
        EnumWindows(kill_window_enum_proc, static_cast<LPARAM>(pid));
        
        // Send CTRL_BREAK as a soft interrupt for console processes
        GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid);

        // Fallback: Terminate process tree if not terminated within 250ms
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        DWORD code = 0;
        if (GetExitCodeProcess(hProcess, &code) && code == STILL_ACTIVE) {
            if (hJob != INVALID_HANDLE_VALUE && hJob != NULL) {
                TerminateJobObject(hJob, EXIT_TIMEOUT_EXPIRED);
            } else {
                terminate_process_tree_snapshot(pid, EXIT_TIMEOUT_EXPIRED);
            }
        }
    }
}

// 
// COMPREHENSIVE HELP & VERSION SECTION
// 

void print_version() {
    std::wcout << L"expire v1.0.0\n"
               << L"Copyright (C) 2026, Roberto J Dohnert. BSD 3-Clause License.\n";
}

void print_help() {
    std::wcout
        << L"Usage: expire [OPTION]... DURATION COMMAND [ARG]...\n"
        << L"   or: expire --help | --version\n\n"
        << L"Start COMMAND, and terminate it if still running after DURATION.\n\n"
        << L"Mandatory arguments to long options are mandatory for short options too.\n"
        << L"  -s, --signal=SIGNAL      specify the signal to send on timeout;\n"
        << L"                           SIGNAL may be a name ('TERM', 'KILL', 'INT', 'HUP', 'QUIT')\n"
        << L"                           or a signal number (1, 2, 3, 9, 15). Default is TERM.\n"
        << L"  -k, --kill-after=DURATION send a force-kill signal (SIGKILL / 9) if COMMAND\n"
        << L"                           is still running this long after the initial signal was sent.\n"
        << L"      --preserve-status    exit with the same status as COMMAND, even when\n"
        << L"                           the command times out.\n"
        << L"  -f, --foreground         allow COMMAND to attach to the foreground console session\n"
        << L"                           and read directly from standard TTY input.\n"
        << L"  -v, --verbose            diagnose to standard error any signal sent upon timeout.\n"
        << L"      --json              output final execution status as JSON.\n"
        << L"      --csv               output final execution status as CSV.\n"
        << L"      --table             output final execution status as a table.\n"
        << L"      --pipe COMMAND      send final execution status through COMMAND.\n"
        << L"      --help               display this comprehensive help manual and exit.\n"
        << L"      --version            output version information and exit.\n\n"
        << L"DURATION Specs:\n"
        << L"  DURATION is a floating-point number with an optional suffix:\n"
        << L"    's' for seconds (default)\n"
        << L"    'm' for minutes\n"
        << L"    'h' for hours\n"
        << L"    'd' for days\n"
        << L"  A duration of 0 disables the associated timeout (infinite wait).\n\n"
        << L"Windows Signal Mapping:\n"
        << L"  Since native Windows processes do not use POSIX signals, signals are emulated:\n"
        << L"    TERM / 15 (Default) -> Graceful WM_CLOSE / CTRL_BREAK + Job Object Tree Terminate\n"
        << L"    KILL / 9            -> Immediate Force Terminate (TerminateJobObject)\n"
        << L"    INT  / 2            -> Win32 CTRL_C_EVENT Signal\n"
        << L"    QUIT / 3            -> Win32 CTRL_BREAK_EVENT Signal\n"
        << L"    HUP  / 1            -> SIGHUP Graceful Console Close\n\n"
        << L"Exit Status:\n"
        << L"  124 : COMMAND timed out and --preserve-status was not set\n"
        << L"  125 : expire internal error or invalid CLI arguments\n"
        << L"  126 : COMMAND was found but could not be invoked\n"
        << L"  127 : COMMAND could not be found\n"
        << L"  137 : COMMAND was force-killed by SIGKILL (128 + 9)\n"
        << L"  otherwise: Exit status of COMMAND\n\n"
        << L"Examples:\n"
        << L"  expire 5s ping -t 127.0.0.1\n"
        << L"      Runs ping for 5 seconds and terminates it.\n\n"
        << L"  expire -s KILL 1.5m my_app.exe --process\n"
        << L"      Runs my_app.exe for 1.5 minutes and sends SIGKILL if still running.\n\n"
        << L"  expire -k 10s 30s long_running_job.exe\n"
        << L"      Sends SIGTERM after 30 seconds. If still alive 10 seconds later, sends SIGKILL.\n\n"
        << L"  expire --preserve-status 10 python script.py\n"
        << L"      Returns python's exit status even if python timed out.\n";
}

// 
// MAIN EXECUTION ENGINE
//

static int expire_main(int argc, wchar_t* argv[]) {
    g_expire_format = 0;
    g_expire_pipe.clear();
    if (argc <= 1) {
        std::wcerr << L"expire: missing operand\n"
                   << L"Try 'expire --help' for more information.\n";
        return EXIT_INTERNAL_ERROR;
    }

    DWORD initial_signal = SIG_TERM;
    double timeout_seconds = 0.0;
    double kill_after_seconds = -1.0;
    bool preserve_status = false;
    bool foreground = false;
    bool verbose = false;

    int cmd_idx = 1;
    bool stop_flags = false;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (!stop_flags && arg == L"--") {
            stop_flags = true;
            cmd_idx = i + 1;
            continue;
        }

        if (!stop_flags && !arg.empty() && arg[0] == L'-' && arg.length() > 1) {
            if (arg == L"--help") {
                print_help();
                return 0;
            }
            if (arg == L"--version") {
                print_version();
                return 0;
            }
            if (arg == L"--json") { g_expire_format = 1; continue; }
            if (arg == L"--csv") { g_expire_format = 2; continue; }
            if (arg == L"--table") { g_expire_format = 3; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { g_expire_pipe = argv[++i]; continue; }
            if (arg == L"--preserve-status") {
                preserve_status = true;
                continue;
            }
            if (arg == L"-f" || arg == L"--foreground") {
                foreground = true;
                continue;
            }
            if (arg == L"-v" || arg == L"--verbose") {
                verbose = true;
                continue;
            }
            if (arg.rfind(L"-s=", 0) == 0 || arg.rfind(L"--signal=", 0) == 0) {
                size_t eq = arg.find(L'=');
                if (!parse_signal(arg.substr(eq + 1), initial_signal)) {
                    std::wcerr << L"expire: invalid signal: " << arg.substr(eq + 1) << L"\n";
                    return EXIT_INTERNAL_ERROR;
                }
                continue;
            }
            if (arg == L"-s" || arg == L"--signal") {
                if (i + 1 >= argc) {
                    std::wcerr << L"expire: option requires an argument -- '" << arg << L"'\n";
                    return EXIT_INTERNAL_ERROR;
                }
                if (!parse_signal(argv[++i], initial_signal)) {
                    std::wcerr << L"expire: invalid signal: " << argv[i] << L"\n";
                    return EXIT_INTERNAL_ERROR;
                }
                continue;
            }
            if (arg.rfind(L"-k=", 0) == 0 || arg.rfind(L"--kill-after=", 0) == 0) {
                size_t eq = arg.find(L'=');
                if (!parse_duration(arg.substr(eq + 1), kill_after_seconds)) {
                    std::wcerr << L"expire: invalid duration: " << arg.substr(eq + 1) << L"\n";
                    return EXIT_INTERNAL_ERROR;
                }
                continue;
            }
            if (arg == L"-k" || arg == L"--kill-after") {
                if (i + 1 >= argc) {
                    std::wcerr << L"expire: option requires an argument -- '" << arg << L"'\n";
                    return EXIT_INTERNAL_ERROR;
                }
                if (!parse_duration(argv[++i], kill_after_seconds)) {
                    std::wcerr << L"expire: invalid duration: " << argv[i] << L"\n";
                    return EXIT_INTERNAL_ERROR;
                }
                continue;
            }

            std::wcerr << L"expire: invalid option -- '" << arg << L"'\n"
                       << L"Try 'expire --help' for more information.\n";
            return EXIT_INTERNAL_ERROR;
        }

        if (!parse_duration(arg, timeout_seconds)) {
            std::wcerr << L"expire: invalid duration: " << arg << L"\n";
            return EXIT_INTERNAL_ERROR;
        }

        cmd_idx = i + 1;
        break;
    }

    if (cmd_idx >= argc) {
        std::wcerr << L"expire: missing command\n"
                   << L"Try 'expire --help' for more information.\n";
        return EXIT_INTERNAL_ERROR;
    }

    std::wstring raw_cmd = argv[cmd_idx];
    std::wstring resolved_exe = resolve_executable(raw_cmd);

    // FIX 1: Detect batch files (.bat / .cmd) and wrap via cmd.exe /c
    bool batch_script = !resolved_exe.empty() && is_batch_file(resolved_exe);
    std::wstring cmdline;

    if (batch_script) {
        cmdline = L"cmd.exe /c " + escape_win32_arg(resolved_exe);
    } else if (!resolved_exe.empty()) {
        cmdline = escape_win32_arg(resolved_exe);
    } else {
        cmdline = escape_win32_arg(raw_cmd);
    }

    for (int k = cmd_idx + 1; k < argc; ++k) {
        cmdline += L" " + escape_win32_arg(argv[k]);
    }

    // Assign process tree to a Win32 Job Object for clean process tree termination
    ScopedHandle hJob(CreateJobObjectW(NULL, NULL));
    if (hJob.is_valid()) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!foreground) {
            jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
        }
        SetInformationJobObject(hJob.get(), JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }

    STARTUPINFOW si = { 0 };
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi = { 0 };
    std::vector<wchar_t> cmd_buf(cmdline.begin(), cmdline.end());
    cmd_buf.push_back(L'\0');

    LPCWSTR lpAppName = (!resolved_exe.empty() && !batch_script) ? resolved_exe.c_str() : NULL;
    DWORD creation_flags = CREATE_SUSPENDED;
    if (!foreground) {
        creation_flags |= CREATE_NEW_PROCESS_GROUP;
    }

    BOOL created = CreateProcessW(lpAppName, cmd_buf.data(), NULL, NULL, TRUE,
                                  creation_flags, NULL, NULL, &si, &pi);

    if (!created) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            std::wcerr << L"expire: failed to run command '" << raw_cmd << L"': No such file or directory\n";
            return EXIT_CMD_NOT_FOUND;
        }
        std::wcerr << L"expire: failed to run command '" << raw_cmd << L"': Access denied or invocation failed\n";
        return EXIT_CANNOT_INVOKE;
    }

    ScopedHandle hProcess(pi.hProcess);
    ScopedHandle hThread(pi.hThread);

    // FIX 2: Gracefully handle nested job assignment failures in CI/CD runners
    bool assigned_to_job = false;
    if (hJob.is_valid()) {
        if (AssignProcessToJobObject(hJob.get(), hProcess.get())) {
            assigned_to_job = true;
        }
    }

    // Ignore CTRL_C in expire process so signals route directly to child
    SetConsoleCtrlHandler(NULL, TRUE);

    ResumeThread(hThread.get());

    HANDLE active_job_handle = assigned_to_job ? hJob.get() : INVALID_HANDLE_VALUE;

    if (timeout_seconds <= 0.0) {
        // Infinite wait (timeout disabled)
        WaitForSingleObject(hProcess.get(), INFINITE);
        DWORD exit_code = 0;
        GetExitCodeProcess(hProcess.get(), &exit_code);
        return static_cast<int>(exit_code);
    }

    DWORD main_timeout_ms = static_cast<DWORD>(timeout_seconds * 1000.0);
    DWORD wait_res = WaitForSingleObject(hProcess.get(), main_timeout_ms);

    if (wait_res == WAIT_OBJECT_0) {
        // Command completed within timeout limit
        DWORD exit_code = 0;
        GetExitCodeProcess(hProcess.get(), &exit_code);
        return static_cast<int>(exit_code);
    }

    // FIX 3: Timeout expired - dispatch signals reliably
    dispatch_win32_signal(active_job_handle, hProcess.get(), pi.dwProcessId, initial_signal, verbose);

    if (kill_after_seconds >= 0.0) {
        DWORD kill_after_ms = static_cast<DWORD>(kill_after_seconds * 1000.0);
        DWORD wait_kill_res = WaitForSingleObject(hProcess.get(), kill_after_ms);
        if (wait_kill_res == WAIT_TIMEOUT) {
            dispatch_win32_signal(active_job_handle, hProcess.get(), pi.dwProcessId, SIG_KILL, verbose);
            WaitForSingleObject(hProcess.get(), 1000);
        }
    } else {
        WaitForSingleObject(hProcess.get(), 1000);
    }

    if (preserve_status) {
        DWORD exit_code = 0;
        GetExitCodeProcess(hProcess.get(), &exit_code);
        return static_cast<int>(exit_code);
    }

    return EXIT_TIMEOUT_EXPIRED;
}

class ExpireApplication {
public:
    int run(int argc, wchar_t* argv[]) const {
        int result = expire_main(argc, argv);
        if (g_expire_format || !g_expire_pipe.empty()) {
            std::wstring text = g_expire_format == 1 ? L"{\"status\":\"completed\",\"exit_code\":" + std::to_wstring(result) + L"}\n" : g_expire_format == 2 ? L"status,exit_code\ncompleted," + std::to_wstring(result) + L"\n" : L"STATUS\tEXIT_CODE\ncompleted\t" + std::to_wstring(result) + L"\n";
            if (!g_expire_pipe.empty()) { FILE* pipe = _wpopen(g_expire_pipe.c_str(), L"w"); if (!pipe) return EXIT_INTERNAL_ERROR; std::string narrow = expire_to_utf8(text); fwrite(narrow.data(), 1, narrow.size(), pipe); _pclose(pipe); }
            else std::wcout << text;
        }
        return result;
    }
};

int wmain(int argc, wchar_t* argv[]) { return ExpireApplication().run(argc, argv); }
