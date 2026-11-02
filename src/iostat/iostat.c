/**
 * Iostat - CrossShell for UNIX
 * Copyright (C) 2026, PC/OpenSystems LLC
 * Licensed under the terms of the BSD 3-Clause License.
 *
 * Compiles with:
 *   MSVC:  cl.exe /W4 /O2 /Fe:iostat.exe iostat.c
 *   MinGW: gcc -Wall -Wextra -O2 -o iostat.exe iostat.c
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define MAX_DRIVES            64
#define DEFAULT_INTERVAL_SEC  1
#define DEFAULT_COUNT         0  /* 0 = Infinite */
#define HNS_PER_SEC           10000000.0  /* 100-nanosecond intervals per second */

/* Forward Declarations */
typedef struct DiskDevice DiskDevice;
typedef struct DiskDeviceVtbl DiskDeviceVtbl;
typedef struct DiskManager DiskManager;
typedef struct DiskManagerVtbl DiskManagerVtbl;
typedef struct AppConfig AppConfig;
typedef struct AppConfigVtbl AppConfigVtbl;

/* ========================================================================= */
/* CLASS: AppConfig (Handles CLI Parsing, Validation, and Help)              */
/* ========================================================================= */

struct AppConfig {
    const AppConfigVtbl* vptr;
    DWORD intervalSec;
    DWORD count;
    bool extended;      /* -x */
    bool megaBytes;     /* -m */
    bool kiloBytes;     /* -k */
    bool timestamp;     /* -t */
    bool showHelp;      /* -h / --help */
    int targetDrive;    /* -d <drive_num>, -1 for all */
};

struct AppConfigVtbl {
    bool (*Parse)(AppConfig* self, int argc, wchar_t* argv[]);
    void (*PrintHelp)(const AppConfig* self, const wchar_t* progName);
};

static void AppConfig_PrintHelp(const AppConfig* self, const wchar_t* progName) {
    (void)self;
    wprintf(
        L"NAME\n"
        L"    iostat - Report Central Processing Unit (CPU) and I/O statistics for disks.\n\n"
        L"SYNOPSIS\n"
        L"    %ls [OPTIONS] [interval [count]]\n\n"
        L"DESCRIPTION\n"
        L"    The iostat command monitors Windows system input/output device loading\n"
        L"    by observing the time the devices are active in relation to their average\n"
        L"    transfer rates. Emulates standard POSIX/UNIX sysstat iostat.\n\n"
        L"OPTIONS\n"
        L"    -x             Display extended statistics (await, r_await, w_await, %%util).\n"
        L"    -k             Display statistics in Kilobytes per second (Default).\n"
        L"    -m             Display statistics in Megabytes per second.\n"
        L"    -t             Print the timestamp for each report displayed.\n"
        L"    -d <drive>     Display statistics only for a specific PhysicalDrive index.\n"
        L"    -h, --help     Display this comprehensive help screen and exit.\n\n"
        L"PARAMETERS\n"
        L"    interval       Number of seconds between each report.\n"
        L"    count          Number of reports generated before exiting (default: infinite).\n\n"
        L"METRICS EXPLANATION\n"
        L"    Device         Physical disk identifier (e.g., PhysicalDrive0).\n"
        L"    r/s            Number of read requests issued per second.\n"
        L"    w/s            Number of write requests issued per second.\n"
        L"    rkB/s, rMB/s   Kilobytes or Megabytes read per second.\n"
        L"    wkB/s, wMB/s   Kilobytes or Megabytes written per second.\n"
        L"    rrqm/s, wrqm/s Read/Write request split (merged/split) rate per second.\n"
        L"    r_await        Average read request turnaround time in milliseconds.\n"
        L"    w_await        Average write request turnaround time in milliseconds.\n"
        L"    aqu-sz         Average queue length of requests issued to the device.\n"
        L"    %%util          Device saturation percentage (disk busy time).\n\n"
        L"EXAMPLES\n"
        L"    %ls 1 5          Output reports every 1s, stop after 5 iterations.\n"
        L"    %ls -x 2         Extended statistics every 2 seconds.\n"
        L"    %ls -x -m -t 1   Extended output in MB/s with timestamp every second.\n"
        L"    %ls -d 0 1       Monitor only PhysicalDrive0 every 1 second.\n",
        progName, progName, progName, progName, progName
    );
}

static bool AppConfig_Parse(AppConfig* self, int argc, wchar_t* argv[]) {
    int positionalIdx = 0;

    for (int i = 1; i < argc; ++i) {
        wchar_t* arg = argv[i];

        if (wcscmp(arg, L"-h") == 0 || wcscmp(arg, L"--help") == 0 || wcscmp(arg, L"/?") == 0) {
            self->showHelp = true;
            return true;
        } else if (wcscmp(arg, L"-V") == 0 || wcscmp(arg, L"--version") == 0) {
            wprintf(L"iostat 1.1.0\n");
            exit(0);
        } else if (wcscmp(arg, L"-x") == 0) {
            self->extended = true;
        } else if (wcscmp(arg, L"-m") == 0) {
            self->megaBytes = true;
            self->kiloBytes = false;
        } else if (wcscmp(arg, L"-k") == 0) {
            self->kiloBytes = true;
            self->megaBytes = false;
        } else if (wcscmp(arg, L"-t") == 0) {
            self->timestamp = true;
        } else if (wcscmp(arg, L"-d") == 0) {
            if (i + 1 < argc) {
                wchar_t* endPtr = NULL;
                long val = wcstol(argv[++i], &endPtr, 10);
                if (endPtr == argv[i] || val < 0 || val >= MAX_DRIVES) {
                    wprintf(L"Error: Invalid drive index specified with -d.\n");
                    return false;
                }
                self->targetDrive = (int)val;
            } else {
                wprintf(L"Error: Option -d requires a drive number argument.\n");
                return false;
            }
        } else if (arg[0] == L'-') {
            wprintf(L"Error: Unknown option '%ls'. Run with -h for usage.\n", arg);
            return false;
        } else {
            /* Positional interval / count */
            wchar_t* endPtr = NULL;
            long val = wcstol(arg, &endPtr, 10);
            if (endPtr == arg || val <= 0) {
                wprintf(L"Error: Invalid interval/count value '%ls'. Must be positive.\n", arg);
                return false;
            }

            if (positionalIdx == 0) {
                self->intervalSec = (DWORD)val;
                positionalIdx++;
            } else if (positionalIdx == 1) {
                self->count = (DWORD)val;
                positionalIdx++;
            } else {
                wprintf(L"Error: Too many positional arguments provided.\n");
                return false;
            }
        }
    }
    return true;
}

static const AppConfigVtbl g_AppConfigVtbl = {
    .Parse = AppConfig_Parse,
    .PrintHelp = AppConfig_PrintHelp
};

static void AppConfig_Init(AppConfig* self) {
    self->vptr = &g_AppConfigVtbl;
    self->intervalSec = DEFAULT_INTERVAL_SEC;
    self->count = DEFAULT_COUNT;
    self->extended = false;
    self->megaBytes = false;
    self->kiloBytes = true;
    self->timestamp = false;
    self->showHelp = false;
    self->targetDrive = -1;
}

/* ========================================================================= */
/* CLASS: DiskDevice (Encapsulates a single physical drive and its metrics)   */
/* ========================================================================= */

typedef struct {
    double r_per_sec;
    double w_per_sec;
    double r_units_per_sec;
    double w_units_per_sec;
    double r_await_ms;
    double w_await_ms;
    double avg_q_size;
    double split_per_sec;
    double util_pct;
} DiskMetrics;

struct DiskDevice {
    const DiskDeviceVtbl* vptr;
    int driveIndex;
    HANDLE hDevice;
    bool isOnline;
    bool hasPreviousSample;

    DISK_PERFORMANCE prevSample;
    DISK_PERFORMANCE currSample;
    LARGE_INTEGER prevTime;
    LARGE_INTEGER currTime;
    LARGE_INTEGER timerFreq;

    DiskMetrics metrics;
};

struct DiskDeviceVtbl {
    bool (*Open)(DiskDevice* self);
    bool (*Sample)(DiskDevice* self);
    void (*Compute)(DiskDevice* self, const AppConfig* cfg);
    void (*Display)(const DiskDevice* self, const AppConfig* cfg);
    void (*Close)(DiskDevice* self);
};

static bool DiskDevice_Open(DiskDevice* self) {
    if (self->hDevice != INVALID_HANDLE_VALUE) {
        return true;
    }

    wchar_t path[64];
    swprintf_s(path, _countof(path), L"\\\\.\\PhysicalDrive%d", self->driveIndex);

    /* Use 0 for desired access to query metadata without requiring write rights */
    self->hDevice = CreateFileW(
        path,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (self->hDevice == INVALID_HANDLE_VALUE) {
        self->isOnline = false;
        return false;
    }

    self->isOnline = true;
    QueryPerformanceFrequency(&self->timerFreq);
    return true;
}

static void DiskDevice_Close(DiskDevice* self) {
    if (self->hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(self->hDevice);
        self->hDevice = INVALID_HANDLE_VALUE;
    }
    self->isOnline = false;
    self->hasPreviousSample = false;
}

static bool DiskDevice_Sample(DiskDevice* self) {
    if (!self->isOnline && !DiskDevice_Open(self)) {
        return false;
    }

    DISK_PERFORMANCE rawData;
    DWORD bytesReturned = 0;

    BOOL status = DeviceIoControl(
        self->hDevice,
        IOCTL_DISK_PERFORMANCE,
        NULL,
        0,
        &rawData,
        sizeof(rawData),
        &bytesReturned,
        NULL
    );

    if (!status || bytesReturned < sizeof(DISK_PERFORMANCE)) {
        DiskDevice_Close(self);
        return false;
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    if (!self->hasPreviousSample) {
        self->currSample = rawData;
        self->currTime = now;
        self->prevSample = rawData;
        self->prevTime = now;
        self->hasPreviousSample = true;
        return true;
    }

    self->prevSample = self->currSample;
    self->prevTime = self->currTime;
    self->currSample = rawData;
    self->currTime = now;

    return true;
}

static void DiskDevice_Compute(DiskDevice* self, const AppConfig* cfg) {
    memset(&self->metrics, 0, sizeof(DiskMetrics));

    double elapsedSec = 0.0;
    if (self->timerFreq.QuadPart > 0) {
        elapsedSec = (double)(self->currTime.QuadPart - self->prevTime.QuadPart) / 
                     (double)self->timerFreq.QuadPart;
    }

    if (elapsedSec <= 0.000001) {
        return; /* Avoid division by zero on identical timestamps */
    }

    int64_t dReadCount = (int64_t)self->currSample.ReadCount - self->prevSample.ReadCount;
    int64_t dWriteCount = (int64_t)self->currSample.WriteCount - self->prevSample.WriteCount;
    int64_t dBytesRead = self->currSample.BytesRead.QuadPart - self->prevSample.BytesRead.QuadPart;
    int64_t dBytesWritten = self->currSample.BytesWritten.QuadPart - self->prevSample.BytesWritten.QuadPart;
    int64_t dReadTime = self->currSample.ReadTime.QuadPart - self->prevSample.ReadTime.QuadPart;
    int64_t dWriteTime = self->currSample.WriteTime.QuadPart - self->prevSample.WriteTime.QuadPart;
    int64_t dIdleTime = self->currSample.IdleTime.QuadPart - self->prevSample.IdleTime.QuadPart;
    int64_t dSplitCount = (int64_t)self->currSample.SplitCount - self->prevSample.SplitCount;

    /* Mitigate potential counter rollbacks/recalibrations */
    if (dReadCount < 0) dReadCount = 0;
    if (dWriteCount < 0) dWriteCount = 0;
    if (dBytesRead < 0) dBytesRead = 0;
    if (dBytesWritten < 0) dBytesWritten = 0;
    if (dReadTime < 0) dReadTime = 0;
    if (dWriteTime < 0) dWriteTime = 0;
    if (dIdleTime < 0) dIdleTime = 0;
    if (dSplitCount < 0) dSplitCount = 0;

    double divisor = cfg->megaBytes ? (1024.0 * 1024.0) : 1024.0;

    self->metrics.r_per_sec = (double)dReadCount / elapsedSec;
    self->metrics.w_per_sec = (double)dWriteCount / elapsedSec;
    self->metrics.r_units_per_sec = ((double)dBytesRead / divisor) / elapsedSec;
    self->metrics.w_units_per_sec = ((double)dBytesWritten / divisor) / elapsedSec;
    self->metrics.split_per_sec = (double)dSplitCount / elapsedSec;

    /* r_await and w_await in milliseconds (ReadTime is in 100ns units -> / 10,000 for ms) */
    self->metrics.r_await_ms = (dReadCount > 0) 
        ? ((double)dReadTime / 10000.0) / (double)dReadCount 
        : 0.0;

    self->metrics.w_await_ms = (dWriteCount > 0) 
        ? ((double)dWriteTime / 10000.0) / (double)dWriteCount 
        : 0.0;

    self->metrics.avg_q_size = (double)self->currSample.QueueDepth;

    /* %util calculation based on time spent not idle */
    double idleSec = (double)dIdleTime / HNS_PER_SEC;
    double busySec = elapsedSec - idleSec;
    if (busySec < 0.0) busySec = 0.0;
    self->metrics.util_pct = (busySec / elapsedSec) * 100.0;
    if (self->metrics.util_pct > 100.0) {
        self->metrics.util_pct = 100.0;
    }
}

static void DiskDevice_Display(const DiskDevice* self, const AppConfig* cfg) {
    char devName[32];
    snprintf(devName, sizeof(devName), "PhysicalDrive%d", self->driveIndex);

    if (cfg->extended) {
        printf("%-16s %7.2f %7.2f %10.2f %10.2f %8.2f %8.2f %8.2f %7.2f %6.2f\n",
            devName,
            self->metrics.r_per_sec,
            self->metrics.w_per_sec,
            self->metrics.r_units_per_sec,
            self->metrics.w_units_per_sec,
            self->metrics.split_per_sec,
            self->metrics.r_await_ms,
            self->metrics.w_await_ms,
            self->metrics.avg_q_size,
            self->metrics.util_pct
        );
    } else {
        printf("%-16s %8.2f %12.2f %12.2f %12llu %12llu\n",
            devName,
            self->metrics.r_per_sec + self->metrics.w_per_sec,
            self->metrics.r_units_per_sec,
            self->metrics.w_units_per_sec,
            (unsigned long long)self->currSample.BytesRead.QuadPart,
            (unsigned long long)self->currSample.BytesWritten.QuadPart
        );
    }
}

static const DiskDeviceVtbl g_DiskDeviceVtbl = {
    .Open = DiskDevice_Open,
    .Sample = DiskDevice_Sample,
    .Compute = DiskDevice_Compute,
    .Display = DiskDevice_Display,
    .Close = DiskDevice_Close
};

static void DiskDevice_Init(DiskDevice* self, int index) {
    memset(self, 0, sizeof(DiskDevice));
    self->vptr = &g_DiskDeviceVtbl;
    self->driveIndex = index;
    self->hDevice = INVALID_HANDLE_VALUE;
}

/* ========================================================================= */
/* CLASS: DiskManager (Container and orchestrator for DiskDevices)           */
/* ========================================================================= */

struct DiskManager {
    const DiskManagerVtbl* vptr;
    DiskDevice drives[MAX_DRIVES];
    int activeDriveCount;
};

struct DiskManagerVtbl {
    void (*Scan)(DiskManager* self, int targetDrive);
    void (*CollectAll)(DiskManager* self);
    void (*ProcessAndRender)(DiskManager* self, const AppConfig* cfg, bool printHeader);
    void (*Destroy)(DiskManager* self);
};

static void DiskManager_Scan(DiskManager* self, int targetDrive) {
    self->activeDriveCount = 0;

    int start = (targetDrive >= 0) ? targetDrive : 0;
    int end   = (targetDrive >= 0) ? targetDrive + 1 : MAX_DRIVES;

    for (int i = start; i < end; ++i) {
        DiskDevice_Init(&self->drives[i], i);
        if (self->drives[i].vptr->Open(&self->drives[i])) {
            self->activeDriveCount++;
        }
    }
}

static void DiskManager_CollectAll(DiskManager* self) {
    for (int i = 0; i < MAX_DRIVES; ++i) {
        if (self->drives[i].isOnline) {
            self->drives[i].vptr->Sample(&self->drives[i]);
        }
    }
}

static void DiskManager_PrintHeader(const AppConfig* cfg) {
    if (cfg->timestamp) {
        time_t now = time(NULL);
        struct tm tmBuf;
        localtime_s(&tmBuf, &now);
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tmBuf);
        printf("[%s]\n", timeStr);
    }

    const char* unitLabel = cfg->megaBytes ? "MB/s" : "kB/s";

    if (cfg->extended) {
        char rUnit[16], wUnit[16];
        snprintf(rUnit, sizeof(rUnit), "r%s", unitLabel);
        snprintf(wUnit, sizeof(wUnit), "w%s", unitLabel);

        printf("%-16s %7s %7s %10s %10s %8s %8s %8s %7s %6s\n",
            "Device", "r/s", "w/s", rUnit, wUnit, "split/s", "r_await", "w_await", "aqu-sz", "%util");
        printf("-------------------------------------------------------------------------------------------------\n");
    } else {
        char rUnit[16], wUnit[16];
        snprintf(rUnit, sizeof(rUnit), "r%s", unitLabel);
        snprintf(wUnit, sizeof(wUnit), "w%s", unitLabel);

        printf("%-16s %8s %12s %12s %12s %12s\n",
            "Device", "tps", rUnit, wUnit, "r_total", "w_total");
        printf("------------------------------------------------------------------------------\n");
    }
}

static void DiskManager_ProcessAndRender(DiskManager* self, const AppConfig* cfg, bool printHeader) {
    if (printHeader) {
        DiskManager_PrintHeader(cfg);
    } else {
        /* Move the cursor back up over the previous report's data rows so the
           table updates in place (like top/htop) instead of scrolling. */
        int rows = self->activeDriveCount;
        if (rows > 0) {
            printf("\x1b[%dA", rows); /* cursor up N rows */
        }
    }

    int rendered = 0;
    for (int i = 0; i < MAX_DRIVES; ++i) {
        if (self->drives[i].isOnline) {
            self->drives[i].vptr->Compute(&self->drives[i], cfg);
            self->drives[i].vptr->Display(&self->drives[i], cfg);
            rendered++;
        }
    }

    if (rendered == 0) {
        printf("No active physical drives found or access denied (Try running as Administrator).\n");
    }
    /* Flush so each report appears promptly. */
    fflush(stdout);
}

static void DiskManager_Destroy(DiskManager* self) {
    for (int i = 0; i < MAX_DRIVES; ++i) {
        if (self->drives[i].isOnline) {
            self->drives[i].vptr->Close(&self->drives[i]);
        }
    }
    self->activeDriveCount = 0;
}

static const DiskManagerVtbl g_DiskManagerVtbl = {
    .Scan = DiskManager_Scan,
    .CollectAll = DiskManager_CollectAll,
    .ProcessAndRender = DiskManager_ProcessAndRender,
    .Destroy = DiskManager_Destroy
};

static void DiskManager_Init(DiskManager* self) {
    memset(self, 0, sizeof(DiskManager));
    self->vptr = &g_DiskManagerVtbl;
}

/* ========================================================================= */
/* GLOBAL CONTROL & SIGNAL HANDLING                                          */
/* ========================================================================= */

static volatile BOOL g_Running = TRUE;

static BOOL WINAPI SignalHandler(DWORD signalType) {
    if (signalType == CTRL_C_EVENT || signalType == CTRL_BREAK_EVENT || signalType == CTRL_CLOSE_EVENT) {
        g_Running = FALSE;
        return TRUE;
    }
    return FALSE;
}

/* ========================================================================= */
/* ENTRY POINT                                                               */
/* ========================================================================= */

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleCtrlHandler(SignalHandler, TRUE);

    /* Enable ANSI/VT100 escape processing so the in-place table redraw
       (cursor-up) works on Windows ConHost and Windows Terminal. */
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD mode = 0;
            if (GetConsoleMode(hOut, &mode)) {
                SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
    }

    AppConfig config;
    AppConfig_Init(&config);

    if (!config.vptr->Parse(&config, argc, argv)) {
        return EXIT_FAILURE;
    }

    if (config.showHelp) {
        config.vptr->PrintHelp(&config, argv[0]);
        return EXIT_SUCCESS;
    }

    DiskManager manager;
    DiskManager_Init(&manager);
    manager.vptr->Scan(&manager, config.targetDrive);

    if (manager.activeDriveCount == 0) {
        fwprintf(stderr, L"Warning: Unable to open physical drives. Please run the command prompt as Administrator.\n");
    }

    /* Baseline sample to establish initial read/write reference counters */
    manager.vptr->CollectAll(&manager);

    DWORD iterations = 0;

    while (g_Running) {
        Sleep(config.intervalSec * 1000);

        if (!g_Running) {
            break;
        }

        manager.vptr->CollectAll(&manager);
        /* Print the header only on the first report; subsequent reports emit
           only data rows so the output stays as a clean updating table. */
        manager.vptr->ProcessAndRender(&manager, &config, iterations == 0);

        iterations++;
        if (config.count > 0 && iterations >= config.count) {
            break;
        }
    }

    /* Graceful cleanup */
    manager.vptr->Destroy(&manager);
    return EXIT_SUCCESS;
}