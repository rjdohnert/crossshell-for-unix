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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#define KILL_VERSION "1.4.0"

/* Signal definitions and Windows mapping */
typedef struct {
    int         number;
    const char *name;
    const char *description;
} SignalMapping;

static const SignalMapping SUPPORTED_SIGNALS[] = {
    { 0,  "NULL", "Check existence (returns 0 if process is accessible)" },
    { 1,  "HUP",  "Hangup (simulated via WM_CLOSE to top-level windows)" },
    { 2,  "INT",  "Interrupt (sends CTRL_C_EVENT or WM_CLOSE)" },
    { 3,  "QUIT", "Quit (sends CTRL_BREAK_EVENT)" },
    { 9,  "KILL", "Forced termination (calls TerminateProcess unconditionally)" },
    { 15, "TERM", "Software termination signal (sends WM_CLOSE for graceful exit)" }
};

#define NUM_SIGNALS (sizeof(SUPPORTED_SIGNALS) / sizeof(SUPPORTED_SIGNALS[0]))

/* Structure for EnumWindows callback */
typedef struct {
    DWORD pid;
    DWORD window_count;
} WindowEnumContext;

/* -------------------------------------------------------------------------- */
/* Utility: Print friendly Win32 error message                                */
/* -------------------------------------------------------------------------- */
static void PrintSystemError(const char *prefix, DWORD errorCode) {
    LPSTR errorBuffer = NULL;
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&errorBuffer,
        0,
        NULL
    );

    if (size > 0 && errorBuffer != NULL) {
        // Strip trailing carriage return / newlines
        while (size > 0 && (errorBuffer[size - 1] == '\r' || errorBuffer[size - 1] == '\n')) {
            errorBuffer[--size] = '\0';
        }
        fprintf(stderr, "kill: %s: (Error %lu) %s\n", prefix, errorCode, errorBuffer);
        LocalFree(errorBuffer);
    } else {
        fprintf(stderr, "kill: %s: Unknown system error %lu\n", prefix, errorCode);
    }
}

/* -------------------------------------------------------------------------- */
/* Security: Enable SeDebugPrivilege so admins can target other sessions       */
/* -------------------------------------------------------------------------- */
static void EnableDebugPrivilege(void) {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return; // Non-admin or restricted token; continue gracefully
    }

    LUID luid;
    if (LookupPrivilegeValueA(NULL, "SeDebugPrivilege", &luid)) {
        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
    }

    CloseHandle(hToken);
}

/* -------------------------------------------------------------------------- */
/* Callback for EnumWindows: posts WM_CLOSE to every window owned by pid      */
/* -------------------------------------------------------------------------- */
static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    WindowEnumContext *ctx = (WindowEnumContext *)lParam;
    DWORD procId = 0;
    GetWindowThreadProcessId(hwnd, &procId);

    if (procId == ctx->pid) {
        // Post WM_CLOSE to request graceful shutdown
        PostMessageA(hwnd, WM_CLOSE, 0, 0);
        ctx->window_count++;
    }
    return TRUE;
}

/* -------------------------------------------------------------------------- */
/* Signal Resolution Helpers                                                  */
/* -------------------------------------------------------------------------- */
static int ParseSignal(const char *arg) {
    if (!arg || *arg == '\0') return -1;

    // Optional leading hyphen or "SIG" prefix
    if (*arg == '-') arg++;
    if (_strnicmp(arg, "SIG", 3) == 0) arg += 3;

    // Check by signal name
    for (size_t i = 0; i < NUM_SIGNALS; i++) {
        if (_stricmp(arg, SUPPORTED_SIGNALS[i].name) == 0) {
            return SUPPORTED_SIGNALS[i].number;
        }
    }

    // Check numeric signal
    char *endptr = NULL;
    errno = 0;
    long val = strtol(arg, &endptr, 10);
    if (errno == 0 && *endptr == '\0' && endptr != arg) {
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
        printf("%2d) SIG%-5s %s\n",
            SUPPORTED_SIGNALS[i].number,
            SUPPORTED_SIGNALS[i].name,
            SUPPORTED_SIGNALS[i].description
        );
    }
}

/* -------------------------------------------------------------------------- */
/* Signal Delivery on Windows                                                 */
/* -------------------------------------------------------------------------- */
static bool DeliverSignal(DWORD pid, int sig) {
    // Functional & Security Bug Fix: Guard against invalid or critical system PIDs
    if (pid == 0) {
        fprintf(stderr, "kill: (%lu) - Invalid target: PID 0 is the System Idle Process\n", pid);
        return false;
    }
    if (pid == 4) {
        fprintf(stderr, "kill: (%lu) - Refusing to signal NT Kernel System Process\n", pid);
        return false;
    }

    // SIGNAL 0: Check existence only
    if (sig == 0) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess != NULL) {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
                CloseHandle(hProcess);
                return true;
            }
            CloseHandle(hProcess);
            fprintf(stderr, "kill: (%lu) - Process is a zombie or already terminated\n", pid);
            return false;
        }

        DWORD err = GetLastError();
        // If access denied, the process definitely exists
        if (err == ERROR_ACCESS_DENIED) {
            return true;
        }
        PrintSystemError("Process check failed", err);
        return false;
    }

    // SIGNAL 9: Immediate forced termination
    if (sig == 9) {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess == NULL) {
            PrintSystemError("Failed to open process for termination", GetLastError());
            return false;
        }

        // 128 + 9 = 137 standard UNIX exit status for SIGKILL
        if (!TerminateProcess(hProcess, 137)) {
            PrintSystemError("TerminateProcess failed", GetLastError());
            CloseHandle(hProcess);
            return false;
        }

        CloseHandle(hProcess);
        return true;
    }

    // SIGNAL 1 (HUP) or 15 (TERM): Graceful close
    if (sig == 1 || sig == 15) {
        WindowEnumContext ctx = { pid, 0 };
        EnumWindows(EnumWindowsCallback, (LPARAM)&ctx);

        if (ctx.window_count > 0) {
            // Signal posted to GUI windows
            return true;
        }

        // If no GUI windows exist, notify user and attempt console break
        if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid)) {
            fprintf(stderr,
                "kill: PID %lu has no accessible top-level windows; cannot send SIG%s.\n"
                "      Use 'kill -9 %lu' (SIGKILL) to force termination.\n",
                pid, (sig == 1 ? "HUP" : "TERM"), pid);
            return false;
        }
        return true;
    }

    // SIGNAL 2 (SIGINT) / SIGNAL 3 (SIGQUIT): Console events
    if (sig == 2 || sig == 3) {
        DWORD ctrlEvent = (sig == 2) ? CTRL_C_EVENT : CTRL_BREAK_EVENT;
        if (GenerateConsoleCtrlEvent(ctrlEvent, pid)) {
            return true;
        }

        // Fallback: try closing any associated window
        WindowEnumContext ctx = { pid, 0 };
        EnumWindows(EnumWindowsCallback, (LPARAM)&ctx);
        if (ctx.window_count > 0) {
            return true;
        }

        PrintSystemError("GenerateConsoleCtrlEvent failed", GetLastError());
        return false;
    }

    fprintf(stderr, "kill: Unsupported signal %d\n", sig);
    return false;
}

/* -------------------------------------------------------------------------- */
/* Help documentation                                                         */
/* -------------------------------------------------------------------------- */
static void PrintHelp(const char *progName) {
    printf("kill %s\n", KILL_VERSION);
    printf("Usage:\n");
    printf("  %s [-s signal | -n signum | -signal] pid...\n", progName);
    printf("  %s -l [signal]\n", progName);
    printf("  %s -h | --help | /?\n\n", progName);

    printf("Description:\n");
    printf("  Send a signal or termination request to the specified Process ID(s).\n");
    printf("  On Windows, signals are mapped to appropriate Win32 process events.\n\n");

    printf("Options:\n");
    printf("  -s, --signal <sig>   Specify signal by name (e.g., TERM, KILL, 9)\n");
    printf("  -n <signum>          Specify signal by integer number\n");
    printf("  -<sig>               Shorthand for signal (e.g., -9, -TERM, -SIGKILL)\n");
    printf("  -l, --list           List supported signals and their Windows mappings\n");
    printf("  -h, --help, /?       Show this comprehensive help screen and exit\n\n");

    printf("Supported Signals:\n");
    printf("  0  SIGNULL   Check if process exists and is accessible\n");
    printf("  1  SIGHUP    Hangup: sends WM_CLOSE to all top-level windows\n");
    printf("  2  SIGINT    Interrupt: sends CTRL_C_EVENT or WM_CLOSE\n");
    printf("  3  SIGQUIT   Quit: sends CTRL_BREAK_EVENT\n");
    printf("  9  SIGKILL   Kill: unconditional termination (TerminateProcess)\n");
    printf("  15 SIGTERM   Terminate: graceful close via WM_CLOSE (DEFAULT)\n\n");

    printf("Security & Operational Notes:\n");
    printf("  - If running as Administrator, this utility automatically acquires\n");
    printf("    'SeDebugPrivilege' to manage cross-session and background processes.\n");
    printf("  - Target PID 0 (Idle) and PID 4 (System) are protected from signaling.\n\n");

    printf("Examples:\n");
    printf("  %s 1420               Send SIGTERM (15) to PID 1420\n", progName);
    printf("  %s -9 1420 5880       Force terminate (SIGKILL) PIDs 1420 and 5880\n", progName);
    printf("  %s -s KILL 1420       Same as above using signal name\n", progName);
    printf("  %s -0 1420            Test whether PID 1420 is currently running\n", progName);
    printf("  %s -l                 Display list of supported signals\n", progName);
}

/* -------------------------------------------------------------------------- */
/* Main entry point                                                           */
/* -------------------------------------------------------------------------- */
int main(int argc, char **argv) {
    if (argc < 2) {
        PrintHelp(argv[0]);
        return 1;
    }

    EnableDebugPrivilege();

    int targetSignal = 15; // POSIX default is SIGTERM
    int argIndex = 1;

    // Check for help flags
    if (_stricmp(argv[1], "-h") == 0 ||
        _stricmp(argv[1], "--help") == 0 ||
        strcmp(argv[1], "/?") == 0) {
        PrintHelp(argv[0]);
        return 0;
    }

    // Check for list flags (-l or -L)
    if (_stricmp(argv[1], "-l") == 0 || _stricmp(argv[1], "-L") == 0 ||
        _stricmp(argv[1], "--list") == 0) {
        if (argc == 2) {
            PrintSignalList();
            return 0;
        } else {
            // Optional: convert signal spec to number/name
            int sig = ParseSignal(argv[2]);
            if (sig == -1) {
                fprintf(stderr, "kill: %s: unknown signal\n", argv[2]);
                return 1;
            }
            for (size_t i = 0; i < NUM_SIGNALS; i++) {
                if (SUPPORTED_SIGNALS[i].number == sig) {
                    printf("%s\n", SUPPORTED_SIGNALS[i].name);
                    return 0;
                }
            }
            return 1;
        }
    }

    // Parse signal options
    while (argIndex < argc && argv[argIndex][0] == '-') {
        if (strcmp(argv[argIndex], "--") == 0) {
            argIndex++;
            break;
        }

        if (_stricmp(argv[argIndex], "-s") == 0 || _stricmp(argv[argIndex], "--signal") == 0) {
            if (argIndex + 1 >= argc) {
                fprintf(stderr, "kill: option '%s' requires an argument\n", argv[argIndex]);
                return 1;
            }
            targetSignal = ParseSignal(argv[++argIndex]);
            if (targetSignal == -1) {
                fprintf(stderr, "kill: %s: invalid signal specification\n", argv[argIndex]);
                return 1;
            }
            argIndex++;
            break;
        }

        if (_stricmp(argv[argIndex], "-n") == 0) {
            if (argIndex + 1 >= argc) {
                fprintf(stderr, "kill: option '-n' requires an argument\n");
                return 1;
            }
            targetSignal = ParseSignal(argv[++argIndex]);
            if (targetSignal == -1) {
                fprintf(stderr, "kill: %s: invalid signal specification\n", argv[argIndex]);
                return 1;
            }
            argIndex++;
            break;
        }

        // Shorthand syntax like -9 or -KILL
        int sig = ParseSignal(argv[argIndex]);
        if (sig != -1) {
            targetSignal = sig;
            argIndex++;
            break;
        } else {
            fprintf(stderr, "kill: unknown option or invalid signal: %s\n", argv[argIndex]);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return 1;
        }
    }

    if (argIndex >= argc) {
        fprintf(stderr, "kill: no PID specified\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    int failureCount = 0;

    // Process all provided PIDs
    for (int i = argIndex; i < argc; i++) {
        char *endptr = NULL;
        errno = 0;
        unsigned long pid = strtoul(argv[i], &endptr, 10);

        // Security / Functional check: input validation & integer overflow
        if (errno != 0 || *endptr != '\0' || endptr == argv[i]) {
            fprintf(stderr, "kill: '%s': invalid process ID\n", argv[i]);
            failureCount++;
            continue;
        }

        if (pid > MAXDWORD) {
            fprintf(stderr, "kill: '%s': PID out of range\n", argv[i]);
            failureCount++;
            continue;
        }

        if (!DeliverSignal((DWORD)pid, targetSignal)) {
            failureCount++;
        }
    }

    return (failureCount == 0) ? 0 : 1;
}