#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *DAYS_OF_WEEK[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};

// Formats timestamp as: "Day, MM/DD/YYYY hh:mm:ss AM/PM"
static void get_timestamp(char *buf, size_t size) {
    SYSTEMTIME st;
    GetLocalTime(&st);

    int hour12 = st.wHour % 12;
    if (hour12 == 0) hour12 = 12;
    const char *meridiem = (st.wHour >= 12) ? "PM" : "AM";

    snprintf(buf, size, "%s, %02d/%02d/%04d %02d:%02d:%02d %s",
             DAYS_OF_WEEK[st.wDayOfWeek],
             st.wMonth,
             st.wDay,
             st.wYear,
             hour12,
             st.wMinute,
             st.wSecond,
             meridiem);
}

// Convert FILETIME to seconds
static double filetime_to_sec(FILETIME ft) {
    ULARGE_INTEGER li;
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    return (double)li.QuadPart / 10000000.0; // 100-nanosecond intervals
}

// Format duration as Unix standard: "0m0.000s"
static void format_duration(char *buf, size_t size, double total_seconds) {
    int mins = (int)(total_seconds / 60.0);
    double secs = total_seconds - (mins * 60.0);
    snprintf(buf, size, "%dm%.3fs", mins, secs);
}

// Build a command line string from arguments
static char *build_command_line(int argc, char *argv[]) {
    size_t total_len = 0;
    for (int i = 1; i < argc; i++) {
        total_len += strlen(argv[i]) + 3; // space + possible quotes
    }

    char *cmd = (char *)malloc(total_len + 1);
    if (!cmd) return NULL;
    cmd[0] = '\0';

    char *out = cmd;
    for (int i = 1; i < argc; i++) {
        int has_space = (strchr(argv[i], ' ') != NULL);
        size_t arg_len = strlen(argv[i]);
        if (i > 1) *out++ = ' ';
        if (has_space) *out++ = '\"';
        memcpy(out, argv[i], arg_len);
        out += arg_len;
        if (has_space) *out++ = '\"';
    }
    *out = '\0';
    return cmd;
}

int main(int argc, char *argv[]) {
    char stamp[64];

    // If run without arguments: Display the current date and time
    if (argc < 2) {
        get_timestamp(stamp, sizeof(stamp));
        printf("%s\n", stamp);
        return 0;
    }

    // Help flag
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        printf("Usage: clock [command [args...]]\n\n"
               "When run without arguments, prints the current date and time.\n"
               "When given a command, benchmarks its execution and prints\n"
               "real, user, and kernel times along with timestamps.\n");
        return 0;
    }

    char *cmdline = build_command_line(argc, argv);
    if (!cmdline) {
        fprintf(stderr, "clock: memory allocation failed\n");
        return 1;
    }

    // High-resolution real (wall-clock) timing
    LARGE_INTEGER freq, qpc_start, qpc_end;
    QueryPerformanceFrequency(&freq);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    get_timestamp(stamp, sizeof(stamp));
    fprintf(stderr, "[Started:  %s]\n", stamp);

    QueryPerformanceCounter(&qpc_start);

    // Try starting the executable directly; if not found, execute via cmd.exe
    BOOL success = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    if (!success) {
        // Fallback to cmd.exe /c for internal shell commands (e.g. dir, ping, cls)
        char shell_cmd[4096];
        snprintf(shell_cmd, sizeof(shell_cmd), "cmd.exe /c %s", cmdline);
        success = CreateProcessA(NULL, shell_cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
        if (!success) {
            fprintf(stderr, "clock: cannot run '%s': error %lu\n", argv[1], GetLastError());
            free(cmdline);
            return 1;
        }
    }

    // Wait for process to finish
    WaitForSingleObject(pi.hProcess, INFINITE);
    QueryPerformanceCounter(&qpc_end);

    // Retrieve child exit code
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    // Retrieve process CPU usage times
    FILETIME ft_create, ft_exit, ft_kernel, ft_user;
    GetProcessTimes(pi.hProcess, &ft_create, &ft_exit, &ft_kernel, &ft_user);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    free(cmdline);

    double real_sec = (double)(qpc_end.QuadPart - qpc_start.QuadPart) / (double)freq.QuadPart;
    double user_sec = filetime_to_sec(ft_user);
    double sys_sec  = filetime_to_sec(ft_kernel);

    char real_buf[32], user_buf[32], sys_buf[32];
    format_duration(real_buf, sizeof(real_buf), real_sec);
    format_duration(user_buf, sizeof(user_buf), user_sec);
    format_duration(sys_buf, sizeof(sys_buf), sys_sec);

    get_timestamp(stamp, sizeof(stamp));
    fprintf(stderr, "[Finished: %s]\n\n", stamp);

    // Standard Unix time output (written to stderr)
    fprintf(stderr, "real\t%s\n", real_buf);
    fprintf(stderr, "user\t%s\n", user_buf);
    fprintf(stderr, "sys\t%s\n", sys_buf);

    return (int)exit_code;
}