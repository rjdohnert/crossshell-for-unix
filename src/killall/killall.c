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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <wchar.h>

#define KILL_VERSION L"2.0.0"

/* Signal mappings for Windows */
typedef struct {
    int            number;
    const wchar_t *name;
    const wchar_t *description;
} SignalMapping;

static const SignalMapping SUPPORTED_SIGNALS[] = {
    { 0,  L"NULL", L"Check existence (returns 0 if process is alive and accessible)" },
    { 1,  L"HUP",  L"Hangup (graceful close via WM_CLOSE to top-level windows)" },
    { 2,  L"INT",  L"Terminal interrupt (sends CTRL_C_EVENT or WM_CLOSE)" },
    { 3,  L"QUIT", L"Terminal quit (sends CTRL_BREAK_EVENT or WM_CLOSE)" },
    { 9,  L"KILL", L"Unconditional termination (calls TerminateProcess immediately)" },
    { 15, L"TERM", L"Software termination (WM_CLOSE / graceful shutdown) [DEFAULT]" }
};

#define NUM_SIGNALS (sizeof(SUPPORTED_SIGNALS) / sizeof(SUPPORTED_SIGNALS[0]))

/* Context for EnumWindows */
typedef struct {
    DWORD pid;
    DWORD window_count;
} WindowEnumContext;

/* Global operational flags */
static bool g_verbose       = false;
static bool g_quiet         = false;
static bool g_caseSensitive = false;
static bool g_wait          = false;

/* -------------------------------------------------------------------------- */
/* Security: Print formatted Win32 error strings                              */
/* -------------------------------------------------------------------------- */
static void PrintSystemError(const wchar_t *prefix, DWORD errorCode) {
    if (g_quiet) return;

    LPWSTR errorBuffer = NULL;
    DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&errorBuffer,
        0,
        NULL
    );

    if (size > 0 && errorBuffer != NULL) {
        while (size > 0 && (errorBuffer[size - 1] == L'\r' || errorBuffer[size - 1] == L'\n')) {
            errorBuffer[--size] = L'\0';
        }
        fwprintf(stderr, L"kill: %ls: (Error %lu) %ls\n", prefix, errorCode, errorBuffer);
        LocalFree(errorBuffer);
    } else {
        fwprintf(stderr, L"kill: %ls: Unknown system error %lu\n", prefix, errorCode);
    }
}

/* -------------------------------------------------------------------------- */
/* Security: Acquire SeDebugPrivilege to signal processes in other sessions   */
/* -------------------------------------------------------------------------- */
static void EnableDebugPrivilege(void) {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return; // Token unavailable or unprivileged; continue gracefully
    }

    LUID luid;
    if (LookupPrivilegeValueW(NULL, L"SeDebugPrivilege", &luid)) {
        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
    }

    CloseHandle(hToken);
}

/* -------------------------------------------------------------------------- */
/* Callback for EnumWindows: posts WM_CLOSE to gracefully terminate windows   */
/* -------------------------------------------------------------------------- */
static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    WindowEnumContext *ctx = (WindowEnumContext *)lParam;
    DWORD procId = 0;
    GetWindowThreadProcessId(hwnd, &procId);

    if (procId == ctx->pid) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        ctx->window_count++;
    }
    return TRUE;
}

/* -------------------------------------------------------------------------- */
/* Signal Resolution Helpers                                                  */
/* -------------------------------------------------------------------------- */
static int ParseSignal(const wchar_t *arg) {
    if (!arg || *arg == L'\0') return -1;

    if (*arg == L'-') arg++;
    if (_wcsnicmp(arg, L"SIG", 3) == 0) arg += 3;

    // Check by name
    for (size_t i = 0; i < NUM_SIGNALS; i++) {
        if (_wcsicmp(arg, SUPPORTED_SIGNALS[i].name) == 0) {
            return SUPPORTED_SIGNALS[i].number;
        }
    }

    // Check by number
    wchar_t *endptr = NULL;
    long val = wcstol(arg, &endptr, 10);
    if (*endptr == L'\0' && endptr != arg) {
        for (size_t i = 0; i < NUM_SIGNALS; i++) {
            if (SUPPORTED_SIGNALS[i].number == (int)val) {
                return (int)val;
            }
        }
    }

    return -1;
}

static void PrintSignalList(void) {
    for (size_t i = 0; i < NUM_SIGNALS; i++) {
        wprintf(L"%2d) SIG%-5ls %ls\n",
            SUPPORTED_SIGNALS[i].number,
            SUPPORTED_SIGNALS[i].name,
            SUPPORTED_SIGNALS[i].description
        );
    }
}

/* -------------------------------------------------------------------------- */
/* Process Name Matching Logic                                                */
/* -------------------------------------------------------------------------- */
static bool NameMatches(const wchar_t *procExe, const wchar_t *target) {
    // 1. Direct comparison
    if (g_caseSensitive) {
        if (wcscmp(procExe, target) == 0) return true;
    } else {
        if (_wcsicmp(procExe, target) == 0) return true;
    }

    // 2. Convenience matching: if user did not provide ".exe", check target + ".exe"
    const wchar_t *dot = wcsrchr(target, L'.');
    if (!dot) {
        size_t targetLen = wcslen(target);
        size_t fullLen = targetLen + 4 + 1;
        wchar_t *withExt = (wchar_t *)malloc(fullLen * sizeof(wchar_t));
        if (withExt) {
            swprintf(withExt, fullLen, L"%ls.exe", target);
            bool matched = g_caseSensitive ?
                (wcscmp(procExe, withExt) == 0) :
                (_wcsicmp(procExe, withExt) == 0);
            free(withExt);
            return matched;
        }
    }

    return false;
}

/* -------------------------------------------------------------------------- */
/* Signal Delivery on Windows                                                 */
/* -------------------------------------------------------------------------- */
static bool DeliverSignal(DWORD pid, const wchar_t *name, int sig, HANDLE *outWaitHandle) {
    if (outWaitHandle) *outWaitHandle = NULL;

    // Protection: Never target System Idle (PID 0) or System Kernel (PID 4)
    if (pid == 0 || pid == 4) {
        if (g_verbose) {
            fwprintf(stderr, L"kill: Skipping protected kernel process '%ls' (PID %lu)\n", name, pid);
        }
        return false;
    }

    // Protection: Do not kill our own instance of kill
    if (pid == GetCurrentProcessId()) {
        if (g_verbose) {
            fwprintf(stderr, L"kill: Skipping self '%ls' (PID %lu)\n", name, pid);
        }
        return false;
    }

    // SIGNAL 0: Check existence
    if (sig == 0) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess != NULL) {
            DWORD exitCode = 0;
            bool active = (GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE);
            CloseHandle(hProcess);
            if (active) {
                if (g_verbose) wprintf(L"Process '%ls' (PID %lu) exists and is running.\n", name, pid);
                return true;
            }
        }
        if (GetLastError() == ERROR_ACCESS_DENIED) {
            if (g_verbose) wprintf(L"Process '%ls' (PID %lu) exists (access restricted).\n", name, pid);
            return true;
        }
        return false;
    }

    // Open handle with SYNCHRONIZE if waiting is requested
    DWORD accessFlags = (sig == 9) ? PROCESS_TERMINATE : (PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE);
    if (g_wait) accessFlags |= SYNCHRONIZE;

    HANDLE hProcess = OpenProcess(accessFlags, FALSE, pid);

    // SIGNAL 9: Immediate TerminateProcess
    if (sig == 9) {
        if (!hProcess) {
            PrintSystemError(name, GetLastError());
            return false;
        }

        // Standard UNIX exit status for SIGKILL: 128 + 9 = 137
        if (!TerminateProcess(hProcess, 137)) {
            PrintSystemError(name, GetLastError());
            CloseHandle(hProcess);
            return false;
        }

        if (g_verbose) {
            wprintf(L"Killed '%ls' (PID %lu) with SIGKILL.\n", name, pid);
        }

        if (g_wait && outWaitHandle) {
            *outWaitHandle = hProcess;
        } else {
            CloseHandle(hProcess);
        }
        return true;
    }

    // SIGNAL 1 (HUP) or 15 (TERM): Graceful termination via WM_CLOSE
    if (sig == 1 || sig == 15) {
        WindowEnumContext ctx = { pid, 0 };
        EnumWindows(EnumWindowsCallback, (LPARAM)&ctx);

        if (ctx.window_count > 0) {
            if (g_verbose) {
                wprintf(L"Sent WM_CLOSE to %lu window(s) of '%ls' (PID %lu).\n", ctx.window_count, name, pid);
            }
            if (g_wait && hProcess && outWaitHandle) {
                *outWaitHandle = hProcess;
            } else if (hProcess) {
                CloseHandle(hProcess);
            }
            return true;
        }

        // Fallback for console processes without windows
        if (GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid)) {
            if (g_verbose) {
                wprintf(L"Sent CTRL_BREAK_EVENT to console process '%ls' (PID %lu).\n", name, pid);
            }
            if (g_wait && hProcess && outWaitHandle) {
                *outWaitHandle = hProcess;
            } else if (hProcess) {
                CloseHandle(hProcess);
            }
            return true;
        }

        if (!g_quiet) {
            fwprintf(stderr, L"kill: '%ls' (PID %lu) has no top-level windows; cannot send SIG%ls.\n"
                             L"      Use 'kill -9 %ls' to force termination.\n",
                     name, pid, (sig == 1 ? L"HUP" : L"TERM"), name);
        }
        if (hProcess) CloseHandle(hProcess);
        return false;
    }

    // SIGNAL 2 (SIGINT) or 3 (SIGQUIT)
    if (sig == 2 || sig == 3) {
        DWORD ctrlEvent = (sig == 2) ? CTRL_C_EVENT : CTRL_BREAK_EVENT;
        if (GenerateConsoleCtrlEvent(ctrlEvent, pid)) {
            if (g_verbose) {
                wprintf(L"Sent console event to '%ls' (PID %lu).\n", name, pid);
            }
            if (hProcess) CloseHandle(hProcess);
            return true;
        }

        // Fallback to WM_CLOSE
        WindowEnumContext ctx = { pid, 0 };
        EnumWindows(EnumWindowsCallback, (LPARAM)&ctx);
        if (ctx.window_count > 0) {
            if (g_verbose) {
                wprintf(L"Sent WM_CLOSE fallback to '%ls' (PID %lu).\n", name, pid);
            }
            if (hProcess) CloseHandle(hProcess);
            return true;
        }

        PrintSystemError(name, GetLastError());
        if (hProcess) CloseHandle(hProcess);
        return false;
    }

    if (hProcess) CloseHandle(hProcess);
    return false;
}

/* -------------------------------------------------------------------------- */
/* Help & Documentation Screen                                                */
/* -------------------------------------------------------------------------- */
static void PrintHelp(const wchar_t *progName) {
    wprintf(L"kill (UNIX killall clone for Windows) version %ls\n", KILL_VERSION);
    wprintf(L"Usage:\n");
    wprintf(L"  %ls [options] [-s signal | -n signum | -signal] process_name...\n", progName);
    wprintf(L"  %ls -l | --list\n", progName);
    wprintf(L"  %ls -h | --help | /?\n\n", progName);

    wprintf(L"Description:\n");
    wprintf(L"  Kills processes by executable name rather than process ID.\n");
    wprintf(L"  If no signal is specified, SIGTERM (15) is sent by default.\n");
    wprintf(L"  If an extension is omitted (e.g., 'notepad'), '.exe' is automatically checked.\n\n");

    wprintf(L"Options:\n");
    wprintf(L"  -s, --signal <sig>    Specify signal by name (e.g., KILL, TERM, 9)\n");
    wprintf(L"  -n <signum>           Specify signal by number (e.g., 9, 15)\n");
    wprintf(L"  -<sig>                Shorthand signal syntax (e.g., -9, -KILL, -TERM)\n");
    wprintf(L"  -c, --case-sensitive  Match process names case-sensitively (default is insensitive)\n");
    wprintf(L"  -v, --verbose         Report each signal sent and process matched\n");
    wprintf(L"  -q, --quiet           Do not print warnings if processes are not found\n");
    wprintf(L"  -w, --wait            Wait until all killed processes have fully exited\n");
    wprintf(L"  -l, --list            List all supported signals and exit\n");
    wprintf(L"  -h, --help, /?        Display this help message and exit\n\n");

    wprintf(L"Supported Signals:\n");
    wprintf(L"  0  SIGNULL  Check existence without signaling\n");
    wprintf(L"  1  SIGHUP   Hangup: sends WM_CLOSE to top-level windows\n");
    wprintf(L"  2  SIGINT   Interrupt: sends CTRL_C_EVENT or WM_CLOSE\n");
    wprintf(L"  3  SIGQUIT  Quit: sends CTRL_BREAK_EVENT or WM_CLOSE\n");
    wprintf(L"  9  SIGKILL  Forced termination: immediate TerminateProcess (unblockable)\n");
    wprintf(L"  15 SIGTERM  Software termination: graceful WM_CLOSE exit (DEFAULT)\n\n");

    wprintf(L"Security and Safety Features:\n");
    wprintf(L"  - Automatically activates 'SeDebugPrivilege' when elevated.\n");
    wprintf(L"  - Kernel System Idle (PID 0) and System (PID 4) are strictly protected.\n");
    wprintf(L"  - Will never kill its own executing process.\n\n");

    wprintf(L"Examples:\n");
    wprintf(L"  %ls notepad                Gracefully close all notepad instances (SIGTERM)\n", progName);
    wprintf(L"  %ls -9 chrome.exe          Force-kill all Google Chrome instances\n", progName);
    wprintf(L"  %ls -s KILL msedge         Force-kill Microsoft Edge\n", progName);
    wprintf(L"  %ls -0 firefox             Check if Firefox is currently running\n", progName);
    wprintf(L"  %ls -w -9 explorer.exe     Force-kill and wait for all instances to exit\n", progName);
    wprintf(L"  %ls -l                     Display list of signals\n", progName);
}

/* -------------------------------------------------------------------------- */
/* Main Entry Point (wmain for Unicode/UTF-16 support)                        */
/* -------------------------------------------------------------------------- */
int wmain(int argc, wchar_t **argv) {
    if (argc < 2) {
        PrintHelp(argv[0]);
        return 1;
    }

    EnableDebugPrivilege();

    int targetSignal = 15; // POSIX default is SIGTERM
    int argIndex = 1;

    // Fast-path help checks
    if (_wcsicmp(argv[1], L"-h") == 0 ||
        _wcsicmp(argv[1], L"--help") == 0 ||
        wcscmp(argv[1], L"/?") == 0) {
        PrintHelp(argv[0]);
        return 0;
    }

    // Fast-path signal list check
    if (_wcsicmp(argv[1], L"-l") == 0 ||
        _wcsicmp(argv[1], L"--list") == 0) {
        PrintSignalList();
        return 0;
    }

    // Parse options
    while (argIndex < argc && argv[argIndex][0] == L'-') {
        if (wcscmp(argv[argIndex], L"--") == 0) {
            argIndex++;
            break;
        }

        if (_wcsicmp(argv[argIndex], L"-v") == 0 || _wcsicmp(argv[argIndex], L"--verbose") == 0) {
            g_verbose = true;
            argIndex++;
            continue;
        }

        if (_wcsicmp(argv[argIndex], L"-q") == 0 || _wcsicmp(argv[argIndex], L"--quiet") == 0) {
            g_quiet = true;
            argIndex++;
            continue;
        }

        if (_wcsicmp(argv[argIndex], L"-c") == 0 || _wcsicmp(argv[argIndex], L"--case-sensitive") == 0) {
            g_caseSensitive = true;
            argIndex++;
            continue;
        }

        if (_wcsicmp(argv[argIndex], L"-w") == 0 || _wcsicmp(argv[argIndex], L"--wait") == 0) {
            g_wait = true;
            argIndex++;
            continue;
        }

        if (_wcsicmp(argv[argIndex], L"-s") == 0 || _wcsicmp(argv[argIndex], L"--signal") == 0) {
            if (argIndex + 1 >= argc) {
                fwprintf(stderr, L"kill: option '%ls' requires an argument\n", argv[argIndex]);
                return 1;
            }
            targetSignal = ParseSignal(argv[++argIndex]);
            if (targetSignal == -1) {
                fwprintf(stderr, L"kill: %ls: invalid signal specification\n", argv[argIndex]);
                return 1;
            }
            argIndex++;
            continue;
        }

        if (_wcsicmp(argv[argIndex], L"-n") == 0) {
            if (argIndex + 1 >= argc) {
                fwprintf(stderr, L"kill: option '-n' requires an argument\n", argv[argIndex]);
                return 1;
            }
            targetSignal = ParseSignal(argv[++argIndex]);
            if (targetSignal == -1) {
                fwprintf(stderr, L"kill: %ls: invalid signal specification\n", argv[argIndex]);
                return 1;
            }
            argIndex++;
            continue;
        }

        // Shorthand signal like -9 or -KILL
        int sig = ParseSignal(argv[argIndex]);
        if (sig != -1) {
            targetSignal = sig;
            argIndex++;
            continue;
        }

        fwprintf(stderr, L"kill: unknown option '%ls'\n", argv[argIndex]);
        fwprintf(stderr, L"Try '%ls --help' for more information.\n", argv[0]);
        return 1;
    }

    if (argIndex >= argc) {
        fwprintf(stderr, L"kill: no process name specified\n");
        fwprintf(stderr, L"Try '%ls --help' for more information.\n", argv[0]);
        return 1;
    }

    // Dynamic list for process wait handles
    HANDLE *waitHandles = NULL;
    size_t waitCount = 0;
    size_t waitCapacity = 0;

    int overallStatus = 0;

    // Process every target name provided on CLI
    for (int t = argIndex; t < argc; t++) {
        const wchar_t *targetName = argv[t];
        DWORD matchedCount = 0;

        // Take a process snapshot
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            PrintSystemError(L"CreateToolhelp32Snapshot failed", GetLastError());
            overallStatus = 1;
            continue;
        }

        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                if (NameMatches(pe.szExeFile, targetName)) {
                    HANDLE hWait = NULL;
                    if (DeliverSignal(pe.th32ProcessID, pe.szExeFile, targetSignal, &hWait)) {
                        matchedCount++;
                        if (g_wait && hWait != NULL) {
                            if (waitCount >= waitCapacity) {
                                waitCapacity = (waitCapacity == 0) ? 16 : waitCapacity * 2;
                                HANDLE *newHandles = (HANDLE *)realloc(waitHandles, waitCapacity * sizeof(HANDLE));
                                if (newHandles) {
                                    waitHandles = newHandles;
                                }
                            }
                            if (waitHandles && waitCount < waitCapacity) {
                                waitHandles[waitCount++] = hWait;
                            } else {
                                CloseHandle(hWait);
                            }
                        }
                    }
                }
            } while (Process32NextW(hSnapshot, &pe));
        }

        CloseHandle(hSnapshot);

        if (matchedCount == 0) {
            if (!g_quiet) {
                fwprintf(stderr, L"kill: %ls: no process found\n", targetName);
            }
            overallStatus = 1;
        }
    }

    // If -w/--wait specified, block until all targeted processes have exited
    if (g_wait && waitCount > 0) {
        if (g_verbose) {
            wprintf(L"Waiting for %zu process(es) to terminate...\n", waitCount);
        }
        for (size_t i = 0; i < waitCount; i++) {
            WaitForSingleObject(waitHandles[i], INFINITE);
            CloseHandle(waitHandles[i]);
        }
    }
    free(waitHandles);

    return overallStatus;
}