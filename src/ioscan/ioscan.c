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

/**
 * @file ioscan.c
 * Copyright (c) 2026, PC/OpenSystems LLC
 * Licensed under the terms of the BSD 3-Clause License.
 *
 * Compiles with:
 *   MSVC:  cl.exe /W4 /O2 /Fe:ioscan.exe ioscan.c setupapi.lib cfgmgr32.lib
 *   MinGW: gcc -Wall -Wextra -O2 -o ioscan.exe ioscan.c -lsetupapi -lcfgmgr32
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wchar.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

/* ========================================================================= */
/* FORWARD DECLARATIONS & INTERFACES                                         */
/* ========================================================================= */

typedef struct DeviceNode DeviceNode;
typedef struct DeviceNodeVtbl DeviceNodeVtbl;

typedef struct DeviceFilter DeviceFilter;
typedef struct DeviceFilterVtbl DeviceFilterVtbl;

typedef struct OutputFormatter OutputFormatter;
typedef struct OutputFormatterVtbl OutputFormatterVtbl;

typedef struct DeviceScanner DeviceScanner;
typedef struct DeviceScannerVtbl DeviceScannerVtbl;

typedef struct AppConfig AppConfig;
typedef struct AppConfigVtbl AppConfigVtbl;

/* ========================================================================= */
/* UTILITY HELPERS (Memory & String Safety)                                  */
/* ========================================================================= */

static wchar_t* SafeWcsDup(const wchar_t* src) {
    if (!src) return NULL;
    size_t len = wcslen(src) + 1;
    wchar_t* dst = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (dst) {
        wcscpy_s(dst, len, src);
    }
    return dst;
}

static bool CaseInsensitiveContains(const wchar_t* haystack, const wchar_t* needle) {
    if (!haystack || !needle) return false;
    size_t hLen = wcslen(haystack);
    size_t nLen = wcslen(needle);
    if (nLen > hLen) return false;

    for (size_t i = 0; i <= hLen - nLen; ++i) {
        if (_wcsnicmp(&haystack[i], needle, nLen) == 0) {
            return true;
        }
    }
    return false;
}

/* ========================================================================= */
/* CLASS: AppConfig (Command Line Parsing & Help Presentation)               */
/* ========================================================================= */

struct AppConfig {
    const AppConfigVtbl* vptr;
    wchar_t* targetClass;
    bool scanKernelOnly;    /* -k : Cached hardware scan */
    bool fullFormat;        /* -f : Verbose detailed table */
    bool compactColon;      /* -F : Colon-delimited script format */
    bool usableOnly;        /* -u : Only online/CLAIMED devices */
    bool showHelp;          /* -h, --help */
};

struct AppConfigVtbl {
    bool (*Parse)(AppConfig* self, int argc, wchar_t* argv[]);
    void (*PrintHelp)(const AppConfig* self, const wchar_t* progName);
    void (*Destroy)(AppConfig* self);
};

static void AppConfig_PrintHelp(const AppConfig* self, const wchar_t* progName) {
    (void)self;
    wprintf(
        L"NAME\n"
        L"    ioscan - Scan and report Windows I/O system hardware devices\n\n"
        L"SYNOPSIS\n"
        L"    %ls [-k] [-f | -F] [-u] [-C class] [-h | --help]\n\n"
        L"DESCRIPTION\n"
        L"    ioscan queries the Windows Plug-and-Play (PnP) subsystem and the NT Kernel\n"
        L"    Configuration Manager to discover hardware devices, inspect driver bindings,\n"
        L"    and report hardware location paths. Emulates UNIX/HP-UX ioscan.\n\n"
        L"OPTIONS\n"
        L"    -k            Scan existing device tree cache (default Windows behavior).\n"
        L"    -f            Generate a full listing with software binding and device state.\n"
        L"    -F            Generate a compact, colon-delimited format suitable for scripts:\n"
        L"                  Class:H/W Path:Driver:S/W State:Description\n"
        L"    -C <class>    Restrict output to devices matching <class> (e.g., disk, net,\n"
        L"                  display, processor, usb, scsi, system).\n"
        L"    -u            Usable devices only: displays only CLAIMED, functional hardware.\n"
        L"    -h, --help    Display this help message and exit.\n\n"
        L"DEVICE STATES (S/W State)\n"
        L"    CLAIMED       Driver successfully loaded and device is fully operational.\n"
        L"    UNCLAIMED     Device detected by bus, but no active driver is servicing it.\n"
        L"    DISABLED      Device disabled by administrator or configuration policy.\n"
        L"    ERROR         Device reported a hardware failure or driver startup error.\n"
        L"    NO_HW         Device definition exists in registry but hardware is detached.\n\n"
        L"EXAMPLES\n"
        L"    %ls -f                  Full scan of all present hardware devices.\n"
        L"    %ls -C disk             Scan and list only disk and storage devices.\n"
        L"    %ls -C net -u           Display only functioning network interfaces.\n"
        L"    %ls -F                  Output entire hardware tree in parseable format.\n",
        progName, progName, progName, progName, progName
    );
}

static bool AppConfig_Parse(AppConfig* self, int argc, wchar_t* argv[]) {
    for (int i = 1; i < argc; ++i) {
        const wchar_t* arg = argv[i];

        if (wcscmp(arg, L"-h") == 0 || wcscmp(arg, L"--help") == 0 || wcscmp(arg, L"/?") == 0) {
            self->showHelp = true;
            return true;
        } else if (wcscmp(arg, L"-k") == 0) {
            self->scanKernelOnly = true;
        } else if (wcscmp(arg, L"-f") == 0) {
            self->fullFormat = true;
            self->compactColon = false;
        } else if (wcscmp(arg, L"-F") == 0) {
            self->compactColon = true;
            self->fullFormat = false;
        } else if (wcscmp(arg, L"-u") == 0) {
            self->usableOnly = true;
        } else if (wcscmp(arg, L"-C") == 0) {
            if (i + 1 < argc) {
                self->targetClass = SafeWcsDup(argv[++i]);
            } else {
                fwprintf(stderr, L"Error: -C option requires a class name argument.\n");
                return false;
            }
        } else {
            fwprintf(stderr, L"Error: Unknown option '%ls'. Run with -h for usage.\n", arg);
            return false;
        }
    }
    return true;
}

static void AppConfig_Destroy(AppConfig* self) {
    if (self->targetClass) {
        free(self->targetClass);
        self->targetClass = NULL;
    }
}

static const AppConfigVtbl g_AppConfigVtbl = {
    .Parse = AppConfig_Parse,
    .PrintHelp = AppConfig_PrintHelp,
    .Destroy = AppConfig_Destroy
};

static void AppConfig_Init(AppConfig* self) {
    self->vptr = &g_AppConfigVtbl;
    self->targetClass = NULL;
    self->scanKernelOnly = false;
    self->fullFormat = false;
    self->compactColon = false;
    self->usableOnly = false;
    self->showHelp = false;
}

/* ========================================================================= */
/* CLASS: DeviceNode (Encapsulates Discovered Hardware Device Attributes)     */
/* ========================================================================= */

struct DeviceNode {
    const DeviceNodeVtbl* vptr;
    wchar_t* hwPath;
    wchar_t* className;
    wchar_t* description;
    wchar_t* driver;
    wchar_t* status;
    bool isUsable;
};

struct DeviceNodeVtbl {
    const wchar_t* (*GetHwPath)(const DeviceNode* self);
    const wchar_t* (*GetClass)(const DeviceNode* self);
    const wchar_t* (*GetDescription)(const DeviceNode* self);
    const wchar_t* (*GetDriver)(const DeviceNode* self);
    const wchar_t* (*GetStatus)(const DeviceNode* self);
    bool (*IsUsable)(const DeviceNode* self);
    void (*Destroy)(DeviceNode* self);
};

static const wchar_t* DeviceNode_GetHwPath(const DeviceNode* self) { return self->hwPath ? self->hwPath : L"N/A"; }
static const wchar_t* DeviceNode_GetClass(const DeviceNode* self) { return self->className ? self->className : L"unknown"; }
static const wchar_t* DeviceNode_GetDescription(const DeviceNode* self) { return self->description ? self->description : L"Unknown Device"; }
static const wchar_t* DeviceNode_GetDriver(const DeviceNode* self) { return self->driver ? self->driver : L"none"; }
static const wchar_t* DeviceNode_GetStatus(const DeviceNode* self) { return self->status ? self->status : L"UNCLAIMED"; }
static bool DeviceNode_IsUsable(const DeviceNode* self) { return self->isUsable; }

static void DeviceNode_Destroy(DeviceNode* self) {
    if (!self) return;
    free(self->hwPath);
    free(self->className);
    free(self->description);
    free(self->driver);
    free(self->status);
    memset(self, 0, sizeof(DeviceNode));
}

static const DeviceNodeVtbl g_DeviceNodeVtbl = {
    .GetHwPath = DeviceNode_GetHwPath,
    .GetClass = DeviceNode_GetClass,
    .GetDescription = DeviceNode_GetDescription,
    .GetDriver = DeviceNode_GetDriver,
    .GetStatus = DeviceNode_GetStatus,
    .IsUsable = DeviceNode_IsUsable,
    .Destroy = DeviceNode_Destroy
};

/* ========================================================================= */
/* CLASS: DeviceFilter (Strategy Pattern for Device Inclusion)               */
/* ========================================================================= */

struct DeviceFilter {
    const DeviceFilterVtbl* vptr;
    const wchar_t* classConstraint;
    bool usableOnly;
};

struct DeviceFilterVtbl {
    bool (*Matches)(const DeviceFilter* self, const DeviceNode* node);
    void (*Destroy)(DeviceFilter* self);
};

static bool DeviceFilter_Matches(const DeviceFilter* self, const DeviceNode* node) {
    if (self->usableOnly && !node->vptr->IsUsable(node)) {
        return false;
    }

    if (self->classConstraint) {
        const wchar_t* nodeClass = node->vptr->GetClass(node);
        if (!CaseInsensitiveContains(nodeClass, self->classConstraint)) {
            return false;
        }
    }

    return true;
}

static void DeviceFilter_Destroy(DeviceFilter* self) {
    (void)self;
}

static const DeviceFilterVtbl g_DeviceFilterVtbl = {
    .Matches = DeviceFilter_Matches,
    .Destroy = DeviceFilter_Destroy
};

static void DeviceFilter_Init(DeviceFilter* self, const wchar_t* classFilter, bool usableOnly) {
    self->vptr = &g_DeviceFilterVtbl;
    self->classConstraint = classFilter;
    self->usableOnly = usableOnly;
}

/* ========================================================================= */
/* INTERFACE & IMPLEMENTATIONS: OutputFormatter (Strategy Pattern)           */
/* ========================================================================= */

struct OutputFormatter {
    const OutputFormatterVtbl* vptr;
};

struct OutputFormatterVtbl {
    void (*PrintHeader)(const OutputFormatter* self);
    void (*PrintRow)(const OutputFormatter* self, const DeviceNode* node);
    void (*PrintFooter)(const OutputFormatter* self, size_t count);
    void (*Destroy)(OutputFormatter* self);
};

/* --- Formatter 1: Default UNIX Table Formatter --- */
static void DefaultFormatter_PrintHeader(const OutputFormatter* self) {
    (void)self;
    wprintf(L"%-32ls %-14ls %-34ls %-12ls\n", L"H/W Path", L"Class", L"Description", L"Status");
    wprintf(L"===============================================================================================\n");
}

static void DefaultFormatter_PrintRow(const OutputFormatter* self, const DeviceNode* node) {
    (void)self;
    wprintf(L"%-32.32ls %-14.14ls %-34.34ls %-12.12ls\n",
        node->vptr->GetHwPath(node),
        node->vptr->GetClass(node),
        node->vptr->GetDescription(node),
        node->vptr->GetStatus(node)
    );
}

static void DefaultFormatter_PrintFooter(const OutputFormatter* self, size_t count) {
    (void)self;
    wprintf(L"===============================================================================================\n");
    wprintf(L"Total devices enumerated: %zu\n\n", count);
}

static void DefaultFormatter_Destroy(OutputFormatter* self) {
    (void)self;
}

static const OutputFormatterVtbl g_DefaultFormatterVtbl = {
    .PrintHeader = DefaultFormatter_PrintHeader,
    .PrintRow = DefaultFormatter_PrintRow,
    .PrintFooter = DefaultFormatter_PrintFooter,
    .Destroy = DefaultFormatter_Destroy
};

/* --- Formatter 2: Full Formatter (-f) --- */
static void FullFormatter_PrintHeader(const OutputFormatter* self) {
    (void)self;
    wprintf(L"%-12ls %-30ls %-14ls %-10ls %-34ls\n", L"Class", L"H/W Path", L"Driver", L"S/W State", L"Description");
    wprintf(L"====================================================================================================================\n");
}

static void FullFormatter_PrintRow(const OutputFormatter* self, const DeviceNode* node) {
    (void)self;
    wprintf(L"%-12.12ls %-30.30ls %-14.14ls %-10.10ls %-34.34ls\n",
        node->vptr->GetClass(node),
        node->vptr->GetHwPath(node),
        node->vptr->GetDriver(node),
        node->vptr->GetStatus(node),
        node->vptr->GetDescription(node)
    );
}

static void FullFormatter_PrintFooter(const OutputFormatter* self, size_t count) {
    (void)self;
    wprintf(L"====================================================================================================================\n");
    wprintf(L"Total devices enumerated: %zu\n\n", count);
}

static const OutputFormatterVtbl g_FullFormatterVtbl = {
    .PrintHeader = FullFormatter_PrintHeader,
    .PrintRow = FullFormatter_PrintRow,
    .PrintFooter = FullFormatter_PrintFooter,
    .Destroy = DefaultFormatter_Destroy
};

/* --- Formatter 3: Compact Colon Formatter (-F) --- */
static void CompactFormatter_PrintHeader(const OutputFormatter* self) { (void)self; }
static void CompactFormatter_PrintFooter(const OutputFormatter* self, size_t count) { (void)self; (void)count; }

static void CompactFormatter_PrintRow(const OutputFormatter* self, const DeviceNode* node) {
    (void)self;
    wprintf(L"%ls:%ls:%ls:%ls:%ls\n",
        node->vptr->GetClass(node),
        node->vptr->GetHwPath(node),
        node->vptr->GetDriver(node),
        node->vptr->GetStatus(node),
        node->vptr->GetDescription(node)
    );
}

static const OutputFormatterVtbl g_CompactFormatterVtbl = {
    .PrintHeader = CompactFormatter_PrintHeader,
    .PrintRow = CompactFormatter_PrintRow,
    .PrintFooter = CompactFormatter_PrintFooter,
    .Destroy = DefaultFormatter_Destroy
};

/* ========================================================================= */
/* CLASS: DeviceScanner (PnP & NT ConfigMgr Hardware Enumerator)             */
/* ========================================================================= */

struct DeviceScanner {
    const DeviceScannerVtbl* vptr;
    DeviceNode* devices;
    size_t count;
    size_t capacity;
};

struct DeviceScannerVtbl {
    bool (*Scan)(DeviceScanner* self);
    size_t (*GetCount)(const DeviceScanner* self);
    const DeviceNode* (*GetDevice)(const DeviceScanner* self, size_t index);
    void (*Destroy)(DeviceScanner* self);
};

/* Property Query Helper - dynamically sized to prevent buffer overflow vulnerabilities */
static wchar_t* QueryDeviceProperty(HDEVINFO devInfo, SP_DEVINFO_DATA* devData, DWORD property) {
    DWORD reqSize = 0;
    SetupDiGetDeviceRegistryPropertyW(devInfo, devData, property, NULL, NULL, 0, &reqSize);
    if (reqSize == 0) {
        return NULL;
    }

    wchar_t* buffer = (wchar_t*)malloc(reqSize);
    if (!buffer) {
        return NULL;
    }

    if (!SetupDiGetDeviceRegistryPropertyW(devInfo, devData, property, NULL, (PBYTE)buffer, reqSize, NULL)) {
        free(buffer);
        return NULL;
    }
    return buffer;
}

static bool DeviceScanner_Append(DeviceScanner* self, DeviceNode* node) {
    if (self->count >= self->capacity) {
        size_t newCap = (self->capacity == 0) ? 64 : self->capacity * 2;
        DeviceNode* newArr = (DeviceNode*)realloc(self->devices, newCap * sizeof(DeviceNode));
        if (!newArr) {
            return false;
        }
        self->devices = newArr;
        self->capacity = newCap;
    }
    self->devices[self->count++] = *node;
    return true;
}

static bool DeviceScanner_Scan(DeviceScanner* self) {
    HDEVINFO devInfo = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (devInfo == INVALID_HANDLE_VALUE) {
        return false;
    }

    SP_DEVINFO_DATA devData;
    devData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devData); ++i) {
        DeviceNode node;
        memset(&node, 0, sizeof(DeviceNode));
        node.vptr = &g_DeviceNodeVtbl;

        /* 1. Retrieve Friendly Name or Description */
        wchar_t* desc = QueryDeviceProperty(devInfo, &devData, SPDRP_FRIENDLYNAME);
        if (!desc) {
            desc = QueryDeviceProperty(devInfo, &devData, SPDRP_DEVICEDESC);
        }
        node.description = desc ? desc : SafeWcsDup(L"Generic PnP Device");

        /* 2. Retrieve Setup Class */
        wchar_t* cls = QueryDeviceProperty(devInfo, &devData, SPDRP_CLASS);
        node.className = cls ? cls : SafeWcsDup(L"system");

        /* 3. Retrieve Hardware Path / Location */
        wchar_t* loc = QueryDeviceProperty(devInfo, &devData, SPDRP_LOCATION_PATHS);
        if (!loc) {
            loc = QueryDeviceProperty(devInfo, &devData, SPDRP_LOCATION_INFORMATION);
        }
        if (!loc) {
            /* Fallback to Device Instance ID if hardware location path is absent */
            wchar_t instId[MAX_DEVICE_ID_LEN] = {0};
            if (CM_Get_Device_IDW(devData.DevInst, instId, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS) {
                loc = SafeWcsDup(instId);
            }
        }
        node.hwPath = loc ? loc : SafeWcsDup(L"N/A");

        /* 4. Retrieve Bound Driver/Service */
        wchar_t* driver = QueryDeviceProperty(devInfo, &devData, SPDRP_SERVICE);
        node.driver = driver ? driver : SafeWcsDup(L"none");

        /* 5. Determine State from NT Configuration Manager */
        ULONG statusFlags = 0;
        ULONG problemNum = 0;
        CONFIGRET cr = CM_Get_DevNode_Status(&statusFlags, &problemNum, devData.DevInst, 0);

        if (cr == CR_SUCCESS) {
            if ((statusFlags & DN_STARTED) && !(statusFlags & DN_HAS_PROBLEM)) {
                node.status = SafeWcsDup(L"CLAIMED");
                node.isUsable = true;
            } else if (statusFlags & DN_HAS_PROBLEM) {
                if (problemNum == CM_PROB_DISABLED) {
                    node.status = SafeWcsDup(L"DISABLED");
                } else {
                    node.status = SafeWcsDup(L"ERROR");
                }
                node.isUsable = false;
            } else {
                node.status = SafeWcsDup(L"UNCLAIMED");
                node.isUsable = false;
            }
        } else if (cr == CR_NO_SUCH_DEVINST) {
            node.status = SafeWcsDup(L"NO_HW");
            node.isUsable = false;
        } else {
            node.status = SafeWcsDup(L"UNCLAIMED");
            node.isUsable = false;
        }

        if (!DeviceScanner_Append(self, &node)) {
            node.vptr->Destroy(&node);
        }
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return true;
}

static size_t DeviceScanner_GetCount(const DeviceScanner* self) {
    return self->count;
}

static const DeviceNode* DeviceScanner_GetDevice(const DeviceScanner* self, size_t index) {
    if (index >= self->count) return NULL;
    return &self->devices[index];
}

static void DeviceScanner_Destroy(DeviceScanner* self) {
    for (size_t i = 0; i < self->count; ++i) {
        self->devices[i].vptr->Destroy(&self->devices[i]);
    }
    free(self->devices);
    self->devices = NULL;
    self->count = 0;
    self->capacity = 0;
}

static const DeviceScannerVtbl g_DeviceScannerVtbl = {
    .Scan = DeviceScanner_Scan,
    .GetCount = DeviceScanner_GetCount,
    .GetDevice = DeviceScanner_GetDevice,
    .Destroy = DeviceScanner_Destroy
};

static void DeviceScanner_Init(DeviceScanner* self) {
    self->vptr = &g_DeviceScannerVtbl;
    self->devices = NULL;
    self->count = 0;
    self->capacity = 0;
}

/* ========================================================================= */
/* ENTRY POINT                                                               */
/* ========================================================================= */

int wmain(int argc, wchar_t* argv[]) {
    /* Initialize Configuration */
    AppConfig config;
    AppConfig_Init(&config);

    if (!config.vptr->Parse(&config, argc, argv)) {
        config.vptr->Destroy(&config);
        return EXIT_FAILURE;
    }

    if (config.showHelp) {
        config.vptr->PrintHelp(&config, argv[0]);
        config.vptr->Destroy(&config);
        return EXIT_SUCCESS;
    }

    /* Instantiate Output Formatter Polymorphically */
    OutputFormatter formatter;
    if (config.compactColon) {
        formatter.vptr = &g_CompactFormatterVtbl;
    } else if (config.fullFormat) {
        formatter.vptr = &g_FullFormatterVtbl;
    } else {
        formatter.vptr = &g_DefaultFormatterVtbl;
    }

    /* Instantiate Hardware Filter */
    DeviceFilter filter;
    DeviceFilter_Init(&filter, config.targetClass, config.usableOnly);

    /* Instantiate & Execute Scanner */
    DeviceScanner scanner;
    DeviceScanner_Init(&scanner);

    if (!scanner.vptr->Scan(&scanner)) {
        fwprintf(stderr, L"Error: Failed to query hardware device subsystem.\n");
        scanner.vptr->Destroy(&scanner);
        filter.vptr->Destroy(&filter);
        formatter.vptr->Destroy(&formatter);
        config.vptr->Destroy(&config);
        return EXIT_FAILURE;
    }

    /* Render Filtered Devices */
    formatter.vptr->PrintHeader(&formatter);

    size_t matchedCount = 0;
    size_t total = scanner.vptr->GetCount(&scanner);
    for (size_t i = 0; i < total; ++i) {
        const DeviceNode* node = scanner.vptr->GetDevice(&scanner, i);
        if (filter.vptr->Matches(&filter, node)) {
            formatter.vptr->PrintRow(&formatter, node);
            matchedCount++;
        }
    }

    formatter.vptr->PrintFooter(&formatter, matchedCount);

    /* Graceful Teardown */
    scanner.vptr->Destroy(&scanner);
    filter.vptr->Destroy(&filter);
    formatter.vptr->Destroy(&formatter);
    config.vptr->Destroy(&config);

    return EXIT_SUCCESS;
}