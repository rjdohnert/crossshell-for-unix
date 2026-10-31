/**
 * machinfo - Advanced System Information Utility for Windows
 * Copyright (c) 2024 PC/OpenSystems LLC. All rights reserved.
 *
 * Description: Advanced system information utility for Windows, providing detailed hardware, OS, and TPM telemetry.
 * 
 * Licensed under the terms of the BSD 3-Clause License.
 * Compilation (MinGW / GCC / Clang):
 *   clang -O2 -Wall machinfo.c -o machinfo.exe -ltbs -liphlpapi -ladvapi32 -lws2_32
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <tbs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#pragma comment(lib, "tbs.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ws2_32.lib")

#define MACHINFO_VERSION "1.4.0"
#define MAX_BUFFER 512
#define MAX_NICS 32

/* TBS result when a standard (non-elevated) user opens a TPM context */
#ifndef TBS_E_ACCESS_DENIED
#define TBS_E_ACCESS_DENIED ((TBS_RESULT)0x8028400F)
#endif

/* ========================================================================= */
/* 1. CLI OPTIONS CLASS                                                      */
/* ========================================================================= */

typedef enum {
    FORMAT_TABLE,
    FORMAT_JSON,
    FORMAT_CSV
} OutputFormat;

typedef struct CliOptions {
    OutputFormat format;
    bool showTpmOnly;
    bool verbose;
    bool noColor;
    bool showHelp;
    bool showVersion;

    /* Member Methods */
    bool (*Parse)(struct CliOptions* this, int argc, char** argv);
    void (*PrintUsage)(const struct CliOptions* this);
} CliOptions;

static void CliOptions_PrintUsage(const CliOptions* this) {
    (void)this;
    wprintf(
        L"NAME\n"
        L"    machinfo - display comprehensive hardware, OS, and security status\n\n"
        L"SYNOPSIS\n"
        L"    machinfo [OPTIONS...]\n\n"
        L"DESCRIPTION\n"
        L"    machinfo is a high-performance, object-oriented utility. It inspects OS configuration, kernel build metrics,\n"
        L"    hardware topology, network adapters, memory utilization, and native\n"
        L"    Trusted Platform Module (TPM 1.2 / 2.0) state via the Windows TBS API.\n\n"
        L"OPTIONS\n"
        L"    -f, --format <table|json|csv>\n"
        L"        Select output layout format. Default is 'table'.\n"
        L"    -t, --tpm-only\n"
        L"        Restrict execution and output strictly to TPM subsystem telemetry.\n"
        L"    -v, --verbose\n"
        L"        Include detailed network adapter configuration and hardware IDs.\n"
        L"    -n, --no-color\n"
        L"        Suppress ANSI terminal escape sequences.\n"
        L"    -V, --version\n"
        L"        Display version information and exit.\n"
        L"    -h, --help\n"
        L"        Print this comprehensive manual page.\n\n"
        L"EXAMPLES\n"
        L"    machinfo --format table\n"
        L"    machinfo -t\n"
        L"    machinfo --format json | jq .System.Memory\n\n"
        L"AUTHOR\n"
        L"    PC/OpenSystems LLC. \n"
    );
}

static bool CliOptions_Parse(CliOptions* this, int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        char* arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            this->showHelp = true;
            return true;
        } else if (strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0) {
            this->showVersion = true;
            return true;
        } else if (strcmp(arg, "-t") == 0 || strcmp(arg, "--tpm-only") == 0) {
            this->showTpmOnly = true;
        } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            this->verbose = true;
        } else if (strcmp(arg, "-n") == 0 || strcmp(arg, "--no-color") == 0) {
            this->noColor = true;
        } else if (strcmp(arg, "-f") == 0 || strcmp(arg, "--format") == 0) {
            if (i + 1 < argc) {
                char* fmt = argv[++i];
                if (_stricmp(fmt, "table") == 0)      this->format = FORMAT_TABLE;
                else if (_stricmp(fmt, "json") == 0)  this->format = FORMAT_JSON;
                else if (_stricmp(fmt, "csv") == 0)   this->format = FORMAT_CSV;
                else {
                    fprintf(stderr, "Error: Unknown format '%s'. Choose table, json, or csv.\n", fmt);
                    return false;
                }
            } else {
                fprintf(stderr, "Error: Option '%s' requires an argument.\n", arg);
                return false;
            }
        } else {
            fprintf(stderr, "Error: Unrecognized option '%s'. Use --help for syntax.\n", arg);
            return false;
        }
    }
    return true;
}

CliOptions* CliOptions_Create(void) {
    CliOptions* obj = (CliOptions*)calloc(1, sizeof(CliOptions));
    if (!obj) return NULL;
    obj->format = FORMAT_TABLE;
    obj->Parse = CliOptions_Parse;
    obj->PrintUsage = CliOptions_PrintUsage;
    return obj;
}

void CliOptions_Destroy(CliOptions* this) {
    if (this) free(this);
}

/* ========================================================================= */
/* 2. DATA MODEL CLASS                                                       */
/* ========================================================================= */

typedef struct NicInfo {
    wchar_t name[MAX_BUFFER];
    wchar_t description[MAX_BUFFER];
    wchar_t macAddress[32];
    wchar_t ipv4Address[64];
    wchar_t ipv6Address[64];
    bool dhcpEnabled;
    bool isUp;
} NicInfo;

typedef struct SystemData {
    /* Host & OS */
    wchar_t hostName[MAX_BUFFER];
    wchar_t osName[MAX_BUFFER];
    wchar_t osVersion[MAX_BUFFER];
    wchar_t osBuild[MAX_BUFFER];
    wchar_t osManufacturer[MAX_BUFFER];
    wchar_t osConfig[MAX_BUFFER];
    wchar_t registeredOwner[MAX_BUFFER];
    wchar_t registeredOrg[MAX_BUFFER];
    wchar_t productId[MAX_BUFFER];
    wchar_t bootTime[MAX_BUFFER];
    wchar_t windowsDir[MAX_BUFFER];
    wchar_t systemDir[MAX_BUFFER];

    /* System & Hardware */
    wchar_t sysManufacturer[MAX_BUFFER];
    wchar_t sysModel[MAX_BUFFER];
    wchar_t sysType[MAX_BUFFER];
    wchar_t processor[MAX_BUFFER];
    wchar_t biosVersion[MAX_BUFFER];
    wchar_t timeZone[MAX_BUFFER];

    /* Memory */
    unsigned long long totalPhysicalMemoryMB;
    unsigned long long availPhysicalMemoryMB;
    unsigned long long totalVirtualMemoryMB;
    unsigned long long availVirtualMemoryMB;

    /* TPM Telemetry */
    bool tpmPresent;
    wchar_t tpmVersion[32];
    wchar_t tpmStatus[MAX_BUFFER];
    wchar_t tpmManufacturer[32];

    /* Network */
    NicInfo nics[MAX_NICS];
    size_t nicCount;
} SystemData;

SystemData* SystemData_Create(void) {
    return (SystemData*)calloc(1, sizeof(SystemData));
}

void SystemData_Destroy(SystemData* this) {
    if (this) free(this);
}

/* ========================================================================= */
/* 3. SYSTEM COLLECTOR CLASS                                                 */
/* ========================================================================= */

typedef struct SystemCollector {
    SystemData* data;

    /* Member Methods */
    void (*CollectAll)(struct SystemCollector* this);
    void (*CollectTpmOnly)(struct SystemCollector* this);
    void (*CollectOsDetails)(struct SystemCollector* this);
    void (*CollectHardwareDetails)(struct SystemCollector* this);
    void (*CollectMemoryDetails)(struct SystemCollector* this);
    void (*CollectTpmDetails)(struct SystemCollector* this);
    void (*CollectNetworkDetails)(struct SystemCollector* this);
} SystemCollector;

/* Helper: Read string from Registry */
static bool QueryRegString(HKEY root, LPCWSTR subKey, LPCWSTR valName, wchar_t* outBuf, DWORD bufSize) {
    HKEY hKey;
    if (RegOpenKeyExW(root, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    DWORD bytes = bufSize * sizeof(wchar_t);
    DWORD type = 0;
    LONG res = RegQueryValueExW(hKey, valName, NULL, &type, (LPBYTE)outBuf, &bytes);
    RegCloseKey(hKey);
    return (res == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ));
}

static void Collector_CollectOsDetails(SystemCollector* this) {
    SystemData* d = this->data;
    DWORD sz = MAX_BUFFER;
    GetComputerNameExW(ComputerNamePhysicalDnsHostname, d->hostName, &sz);

    const LPCWSTR cvKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    QueryRegString(HKEY_LOCAL_MACHINE, cvKey, L"ProductName", d->osName, MAX_BUFFER);
    QueryRegString(HKEY_LOCAL_MACHINE, cvKey, L"DisplayVersion", d->osVersion, MAX_BUFFER);
    QueryRegString(HKEY_LOCAL_MACHINE, cvKey, L"CurrentBuildNumber", d->osBuild, MAX_BUFFER);
    QueryRegString(HKEY_LOCAL_MACHINE, cvKey, L"RegisteredOwner", d->registeredOwner, MAX_BUFFER);
    QueryRegString(HKEY_LOCAL_MACHINE, cvKey, L"RegisteredOrganization", d->registeredOrg, MAX_BUFFER);
    QueryRegString(HKEY_LOCAL_MACHINE, cvKey, L"ProductId", d->productId, MAX_BUFFER);

    wcscpy_s(d->osManufacturer, MAX_BUFFER, L"Microsoft Corporation");
    wcscpy_s(d->osConfig, MAX_BUFFER, L"Standalone Workstation");

    GetWindowsDirectoryW(d->windowsDir, MAX_BUFFER);
    GetSystemDirectoryW(d->systemDir, MAX_BUFFER);

    /* Boot Time Calculation */
    ULONGLONG uptimeMs = GetTickCount64();
    time_t rawtime;
    time(&rawtime);
    time_t bootTimeSec = rawtime - (time_t)(uptimeMs / 1000);
    struct tm bt;
    localtime_s(&bt, &bootTimeSec);
    wcsftime(d->bootTime, MAX_BUFFER, L"%m/%d/%Y, %I:%M:%S %p", &bt);

    /* TimeZone */
    DYNAMIC_TIME_ZONE_INFORMATION dtzi;
    if (GetDynamicTimeZoneInformation(&dtzi) != TIME_ZONE_ID_INVALID) {
        wcscpy_s(d->timeZone, MAX_BUFFER, dtzi.StandardName);
    }
}

static void Collector_CollectHardwareDetails(SystemCollector* this) {
    SystemData* d = this->data;
    const LPCWSTR biosKey = L"HARDWARE\\DESCRIPTION\\System\\BIOS";
    QueryRegString(HKEY_LOCAL_MACHINE, biosKey, L"SystemManufacturer", d->sysManufacturer, MAX_BUFFER);
    QueryRegString(HKEY_LOCAL_MACHINE, biosKey, L"SystemProductName", d->sysModel, MAX_BUFFER);
    QueryRegString(HKEY_LOCAL_MACHINE, biosKey, L"BIOSVersion", d->biosVersion, MAX_BUFFER);

    const LPCWSTR cpuKey = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
    QueryRegString(HKEY_LOCAL_MACHINE, cpuKey, L"ProcessorNameString", d->processor, MAX_BUFFER);

    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            wcscpy_s(d->sysType, MAX_BUFFER, L"x64-based PC");
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            wcscpy_s(d->sysType, MAX_BUFFER, L"ARM64-based PC");
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            wcscpy_s(d->sysType, MAX_BUFFER, L"x86-based PC");
            break;
        default:
            wcscpy_s(d->sysType, MAX_BUFFER, L"Unknown Architecture");
            break;
    }
}

static void Collector_CollectMemoryDetails(SystemCollector* this) {
    SystemData* d = this->data;
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        d->totalPhysicalMemoryMB = ms.ullTotalPhys / (1024 * 1024);
        d->availPhysicalMemoryMB = ms.ullAvailPhys / (1024 * 1024);
        d->totalVirtualMemoryMB  = ms.ullTotalPageFile / (1024 * 1024);
        d->availVirtualMemoryMB  = ms.ullAvailPageFile / (1024 * 1024);
    }
}

static void Collector_CollectTpmDetails(SystemCollector* this) {
    SystemData* d = this->data;

    /* Query TBS Device Information */
    TPM_DEVICE_INFO info;
    memset(&info, 0, sizeof(info));
    info.structVersion = 1;

    TBS_RESULT res = Tbsi_GetDeviceInfo(sizeof(info), &info);
    if (res == TBS_SUCCESS) {
        d->tpmPresent = true;
        if (info.tpmVersion == TPM_VERSION_20) {
            wcscpy_s(d->tpmVersion, 32, L"2.0");
        } else if (info.tpmVersion == TPM_VERSION_12) {
            wcscpy_s(d->tpmVersion, 32, L"1.2");
        } else {
            wcscpy_s(d->tpmVersion, 32, L"Unknown Version");
        }

        /* Check TBS Context Creation to ascertain readiness */
        TBS_CONTEXT_PARAMS params;
        memset(&params, 0, sizeof(params));
        params.version = TBS_CONTEXT_VERSION_ONE;
        TBS_HCONTEXT hContext = 0;

        TBS_RESULT ctxRes = Tbsi_Context_Create(&params, &hContext);
        if (ctxRes == TBS_SUCCESS) {
            wcscpy_s(d->tpmStatus, MAX_BUFFER, L"Enabled, Active, and Ready for Operation");
            Tbsip_Context_Close(hContext);
        } else if (ctxRes == TBS_E_ACCESS_DENIED) {
            wcscpy_s(d->tpmStatus, MAX_BUFFER, L"Present and Enabled (Standard User Access)");
        } else {
            swprintf_s(d->tpmStatus, MAX_BUFFER, L"Present (Status Code: 0x%08X)", ctxRes);
        }
    } else {
        d->tpmPresent = false;
        wcscpy_s(d->tpmVersion, 32, L"N/A");
        if (res == (TBS_RESULT)TBS_E_SERVICE_NOT_RUNNING) {
            wcscpy_s(d->tpmStatus, MAX_BUFFER, L"Not Available (TBS Service Inactive)");
        } else if (res == (TBS_RESULT)TBS_E_TOO_MANY_TBS_CONTEXTS) {
            wcscpy_s(d->tpmStatus, MAX_BUFFER, L"Busy (Exhausted Hardware Contexts)");
        } else {
            wcscpy_s(d->tpmStatus, MAX_BUFFER, L"Not Present / Disabled in Firmware");
        }
    }
}

static void Collector_CollectNetworkDetails(SystemCollector* this) {
    SystemData* d = this->data;
    ULONG bufLen = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(bufLen);

    if (!pAddresses) return;

    DWORD flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST;
    ULONG ret = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, pAddresses, &bufLen);

    if (ret == ERROR_BUFFER_OVERFLOW) {
        free(pAddresses);
        pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(bufLen);
        if (!pAddresses) return;
        ret = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, pAddresses, &bufLen);
    }

    if (ret == NO_ERROR) {
        PIP_ADAPTER_ADDRESSES curr = pAddresses;
        size_t idx = 0;
        while (curr && idx < MAX_NICS) {
            if (curr->IfType != IF_TYPE_SOFTWARE_LOOPBACK) {
                NicInfo* nic = &d->nics[idx];
                wcsncpy_s(nic->name, MAX_BUFFER, curr->FriendlyName, _TRUNCATE);
                wcsncpy_s(nic->description, MAX_BUFFER, curr->Description, _TRUNCATE);
                nic->isUp = (curr->OperStatus == IfOperStatusUp);
                nic->dhcpEnabled = (curr->Dhcpv4Enabled != 0);

                if (curr->PhysicalAddressLength == 6) {
                    swprintf_s(nic->macAddress, 32, L"%02X-%02X-%02X-%02X-%02X-%02X",
                        curr->PhysicalAddress[0], curr->PhysicalAddress[1],
                        curr->PhysicalAddress[2], curr->PhysicalAddress[3],
                        curr->PhysicalAddress[4], curr->PhysicalAddress[5]);
                } else {
                    wcscpy_s(nic->macAddress, 32, L"N/A");
                }

                nic->ipv4Address[0] = L'\0';
                nic->ipv6Address[0] = L'\0';

                for (PIP_ADAPTER_UNICAST_ADDRESS u = curr->FirstUnicastAddress; u != NULL; u = u->Next) {
                    struct sockaddr* sa = u->Address.lpSockaddr;
                    if (sa && sa->sa_family == AF_INET && nic->ipv4Address[0] == L'\0') {
                        char ipStr[64] = {0};
                        inet_ntop(AF_INET, &(((struct sockaddr_in*)sa)->sin_addr), ipStr, sizeof(ipStr));
                        MultiByteToWideChar(CP_UTF8, 0, ipStr, -1, nic->ipv4Address, 64);
                    } else if (sa && sa->sa_family == AF_INET6 && nic->ipv6Address[0] == L'\0') {
                        char ipStr[64] = {0};
                        inet_ntop(AF_INET6, &(((struct sockaddr_in6*)sa)->sin6_addr), ipStr, sizeof(ipStr));
                        MultiByteToWideChar(CP_UTF8, 0, ipStr, -1, nic->ipv6Address, 64);
                    }
                }

                if (nic->ipv4Address[0] == L'\0') {
                    wcscpy_s(nic->ipv4Address, 64, L"Unassigned");
                }
                idx++;
            }
            curr = curr->Next;
        }
        d->nicCount = idx;
    }
    free(pAddresses);
}

static void Collector_CollectAll(SystemCollector* this) {
    this->CollectOsDetails(this);
    this->CollectHardwareDetails(this);
    this->CollectMemoryDetails(this);
    this->CollectTpmDetails(this);
    this->CollectNetworkDetails(this);
}

static void Collector_CollectTpmOnly(SystemCollector* this) {
    DWORD sz = MAX_BUFFER;
    GetComputerNameExW(ComputerNamePhysicalDnsHostname, this->data->hostName, &sz);
    this->CollectTpmDetails(this);
}

SystemCollector* SystemCollector_Create(SystemData* model) {
    SystemCollector* obj = (SystemCollector*)calloc(1, sizeof(SystemCollector));
    if (!obj) return NULL;
    obj->data = model;
    obj->CollectAll = Collector_CollectAll;
    obj->CollectTpmOnly = Collector_CollectTpmOnly;
    obj->CollectOsDetails = Collector_CollectOsDetails;
    obj->CollectHardwareDetails = Collector_CollectHardwareDetails;
    obj->CollectMemoryDetails = Collector_CollectMemoryDetails;
    obj->CollectTpmDetails = Collector_CollectTpmDetails;
    obj->CollectNetworkDetails = Collector_CollectNetworkDetails;
    return obj;
}

void SystemCollector_Destroy(SystemCollector* this) {
    if (this) free(this);
}

/* ========================================================================= */
/* 4. FORMATTER INTERFACE & IMPLEMENTATIONS (POLYMOPRHISM)                    */
/* ========================================================================= */

typedef struct IOutputFormatter {
    const struct IOutputFormatterVtbl* vptr;
    bool noColor;
    bool verbose;
} IOutputFormatter;

typedef struct IOutputFormatterVtbl {
    void (*RenderField)(IOutputFormatter* this, const wchar_t* key, const wchar_t* val, int indent);
    void (*RenderComplete)(IOutputFormatter* this, const SystemData* data, bool tpmOnly);
    void (*Destroy)(IOutputFormatter* this);
} IOutputFormatterVtbl;

/* ---------------- Table / List Formatter ---------------- */
typedef struct TableFormatter {
    IOutputFormatter base;
} TableFormatter;

static void TableFormatter_RenderField(IOutputFormatter* this, const wchar_t* key, const wchar_t* val, int indent) {
    const wchar_t* colorKey = this->noColor ? L"" : L"\x1b[1;36m";
    const wchar_t* colorReset = this->noColor ? L"" : L"\x1b[0m";

    for (int i = 0; i < indent; ++i) wprintf(L"  ");
    wprintf(L"%ls%-28ls%ls: %ls\n", colorKey, key, colorReset, val ? val : L"");
}

static void TableFormatter_RenderComplete(IOutputFormatter* this, const SystemData* d, bool tpmOnly) {
    wchar_t buf[MAX_BUFFER];

    if (tpmOnly) {
        wprintf(L"\n%ls--- Trusted Platform Module (TPM) Telemetry ---%ls\n\n",
            this->noColor ? L"" : L"\x1b[1;32m", this->noColor ? L"" : L"\x1b[0m");
        this->vptr->RenderField(this, L"Host Name", d->hostName, 0);
        this->vptr->RenderField(this, L"TPM Device Present", d->tpmPresent ? L"True" : L"False", 0);
        this->vptr->RenderField(this, L"TPM Specification Version", d->tpmVersion, 0);
        this->vptr->RenderField(this, L"TPM Operational Status", d->tpmStatus, 0);
        wprintf(L"\n");
        return;
    }

    wprintf(L"\n");
    this->vptr->RenderField(this, L"Host Name", d->hostName, 0);
    this->vptr->RenderField(this, L"OS Name", d->osName, 0);
    swprintf_s(buf, MAX_BUFFER, L"%ls (Build %ls)", d->osVersion, d->osBuild);
    this->vptr->RenderField(this, L"OS Version", buf, 0);
    this->vptr->RenderField(this, L"OS Manufacturer", d->osManufacturer, 0);
    this->vptr->RenderField(this, L"OS Configuration", d->osConfig, 0);
    this->vptr->RenderField(this, L"Registered Owner", d->registeredOwner, 0);
    this->vptr->RenderField(this, L"Registered Organization", d->registeredOrg, 0);
    this->vptr->RenderField(this, L"Product ID", d->productId, 0);
    this->vptr->RenderField(this, L"System Boot Time", d->bootTime, 0);
    this->vptr->RenderField(this, L"System Manufacturer", d->sysManufacturer, 0);
    this->vptr->RenderField(this, L"System Model", d->sysModel, 0);
    this->vptr->RenderField(this, L"System Type", d->sysType, 0);
    this->vptr->RenderField(this, L"Processor(s)", d->processor, 0);
    this->vptr->RenderField(this, L"BIOS Version", d->biosVersion, 0);
    this->vptr->RenderField(this, L"Windows Directory", d->windowsDir, 0);
    this->vptr->RenderField(this, L"System Directory", d->systemDir, 0);
    this->vptr->RenderField(this, L"Time Zone", d->timeZone, 0);

    /* Memory Block */
    swprintf_s(buf, MAX_BUFFER, L"%llu MB", d->totalPhysicalMemoryMB);
    this->vptr->RenderField(this, L"Total Physical Memory", buf, 0);
    swprintf_s(buf, MAX_BUFFER, L"%llu MB", d->availPhysicalMemoryMB);
    this->vptr->RenderField(this, L"Available Physical Memory", buf, 0);
    swprintf_s(buf, MAX_BUFFER, L"%llu MB", d->totalVirtualMemoryMB);
    this->vptr->RenderField(this, L"Virtual Memory: Max Size", buf, 0);
    swprintf_s(buf, MAX_BUFFER, L"%llu MB", d->availVirtualMemoryMB);
    this->vptr->RenderField(this, L"Virtual Memory: Available", buf, 0);

    /* TPM Block */
    this->vptr->RenderField(this, L"TPM Device Present", d->tpmPresent ? L"True" : L"False", 0);
    this->vptr->RenderField(this, L"TPM Version", d->tpmVersion, 0);
    this->vptr->RenderField(this, L"TPM Status", d->tpmStatus, 0);

    /* Network Card(s) Block */
    swprintf_s(buf, MAX_BUFFER, L"%zu NIC(s) Installed.", d->nicCount);
    this->vptr->RenderField(this, L"Network Card(s)", buf, 0);

    for (size_t i = 0; i < d->nicCount; ++i) {
        swprintf_s(buf, MAX_BUFFER, L"[%02zu]: %ls", i + 1, d->nics[i].name);
        this->vptr->RenderField(this, buf, L"", 1);
        this->vptr->RenderField(this, L"Connection Status", d->nics[i].isUp ? L"Operational" : L"Disconnected", 2);
        this->vptr->RenderField(this, L"Hardware MAC", d->nics[i].macAddress, 2);
        this->vptr->RenderField(this, L"IPv4 Address", d->nics[i].ipv4Address, 2);
        if (d->nics[i].ipv6Address[0] != L'\0' && (this->verbose || d->nics[i].isUp)) {
            this->vptr->RenderField(this, L"IPv6 Address", d->nics[i].ipv6Address, 2);
        }
        if (this->verbose) {
            this->vptr->RenderField(this, L"DHCP Configured", d->nics[i].dhcpEnabled ? L"Yes" : L"No", 2);
            this->vptr->RenderField(this, L"Driver Description", d->nics[i].description, 2);
        }
    }
    wprintf(L"\n");
}

static void TableFormatter_Destroy(IOutputFormatter* this) {
    if (this) free(this);
}

static const IOutputFormatterVtbl TableVtbl = {
    TableFormatter_RenderField,
    TableFormatter_RenderComplete,
    TableFormatter_Destroy
};

IOutputFormatter* TableFormatter_Create(bool noColor, bool verbose) {
    TableFormatter* f = (TableFormatter*)calloc(1, sizeof(TableFormatter));
    if (!f) return NULL;
    f->base.vptr = &TableVtbl;
    f->base.noColor = noColor;
    f->base.verbose = verbose;
    return (IOutputFormatter*)f;
}

/* ---------------- JSON Formatter ---------------- */
typedef struct JsonFormatter {
    IOutputFormatter base;
} JsonFormatter;

/* Escape a wide string for safe embedding in JSON/CSV double-quoted fields.
   Writes into outBuf (always NUL-terminated). */
static void EscapeJsonString(const wchar_t* in, wchar_t* outBuf, size_t outSize) {
    size_t o = 0;
    if (outSize == 0) return;
    for (const wchar_t* p = in; *p && o + 7 < outSize; ++p) {
        wchar_t c = *p;
        const wchar_t* esc = NULL;
        switch (c) {
            case L'"':  esc = L"\\\""; break;
            case L'\\': esc = L"\\\\"; break;
            case L'\b': esc = L"\\b";  break;
            case L'\f': esc = L"\\f";  break;
            case L'\n': esc = L"\\n";  break;
            case L'\r': esc = L"\\r";  break;
            case L'\t': esc = L"\\t";  break;
            default:
                if (c < 0x20) {
                    swprintf_s(outBuf + o, outSize - o, L"\\u%04x", c);
                    o += 6;
                    continue;
                }
                outBuf[o++] = c;
                continue;
        }
        size_t el = wcslen(esc);
        if (o + el >= outSize) break;
        wcscpy_s(outBuf + o, outSize - o, esc);
        o += el;
    }
    outBuf[o] = L'\0';
}

static void JsonFormatter_RenderComplete(IOutputFormatter* this, const SystemData* d, bool tpmOnly) {
    (void)this;
    wchar_t eHost[MAX_BUFFER * 2], eVer[64], eStat[MAX_BUFFER * 2];
    EscapeJsonString(d->hostName, eHost, MAX_BUFFER * 2);
    EscapeJsonString(d->tpmVersion, eVer, 64);
    EscapeJsonString(d->tpmStatus, eStat, MAX_BUFFER * 2);

    if (tpmOnly) {
        wprintf(
            L"{\n"
            L"  \"HostName\": \"%ls\",\n"
            L"  \"TPM\": {\n"
            L"    \"Present\": %ls,\n"
            L"    \"Version\": \"%ls\",\n"
            L"    \"Status\": \"%ls\"\n"
            L"  }\n"
            L"}\n",
            eHost,
            d->tpmPresent ? L"true" : L"false",
            eVer,
            eStat
        );
        return;
    }

    wchar_t eOsName[MAX_BUFFER * 2], eOsVer[MAX_BUFFER * 2], eOsBuild[MAX_BUFFER * 2];
    wchar_t eOsMfr[MAX_BUFFER * 2], eSysMfr[MAX_BUFFER * 2], eSysModel[MAX_BUFFER * 2];
    wchar_t eSysType[MAX_BUFFER * 2], eProc[MAX_BUFFER * 2], eBoot[MAX_BUFFER * 2];
    EscapeJsonString(d->osName, eOsName, MAX_BUFFER * 2);
    EscapeJsonString(d->osVersion, eOsVer, MAX_BUFFER * 2);
    EscapeJsonString(d->osBuild, eOsBuild, MAX_BUFFER * 2);
    EscapeJsonString(d->osManufacturer, eOsMfr, MAX_BUFFER * 2);
    EscapeJsonString(d->sysManufacturer, eSysMfr, MAX_BUFFER * 2);
    EscapeJsonString(d->sysModel, eSysModel, MAX_BUFFER * 2);
    EscapeJsonString(d->sysType, eSysType, MAX_BUFFER * 2);
    EscapeJsonString(d->processor, eProc, MAX_BUFFER * 2);
    EscapeJsonString(d->bootTime, eBoot, MAX_BUFFER * 2);

    wprintf(
        L"{\n"
        L"  \"HostName\": \"%ls\",\n"
        L"  \"OS\": {\n"
        L"    \"Name\": \"%ls\",\n"
        L"    \"Version\": \"%ls\",\n"
        L"    \"Build\": \"%ls\",\n"
        L"    \"Manufacturer\": \"%ls\"\n"
        L"  },\n"
        L"  \"System\": {\n"
        L"    \"Manufacturer\": \"%ls\",\n"
        L"    \"Model\": \"%ls\",\n"
        L"    \"Architecture\": \"%ls\",\n"
        L"    \"Processor\": \"%ls\",\n"
        L"    \"BootTime\": \"%ls\"\n"
        L"  },\n"
        L"  \"TPM\": {\n"
        L"    \"Present\": %ls,\n"
        L"    \"Version\": \"%ls\",\n"
        L"    \"Status\": \"%ls\"\n"
        L"  },\n"
        L"  \"Memory\": {\n"
        L"    \"TotalPhysicalMB\": %llu,\n"
        L"    \"AvailPhysicalMB\": %llu,\n"
        L"    \"TotalVirtualMB\": %llu,\n"
        L"    \"AvailVirtualMB\": %llu\n"
        L"  },\n"
        L"  \"Network\": [\n",
        eHost, eOsName, eOsVer, eOsBuild, eOsMfr,
        eSysMfr, eSysModel, eSysType, eProc, eBoot,
        d->tpmPresent ? L"true" : L"false", eVer, eStat,
        d->totalPhysicalMemoryMB, d->availPhysicalMemoryMB,
        d->totalVirtualMemoryMB, d->availVirtualMemoryMB
    );

    for (size_t i = 0; i < d->nicCount; ++i) {
        wchar_t eName[MAX_BUFFER * 2], eMac[64], eV4[128], eV6[128], eDesc[MAX_BUFFER * 2];
        EscapeJsonString(d->nics[i].name, eName, MAX_BUFFER * 2);
        EscapeJsonString(d->nics[i].macAddress, eMac, 64);
        EscapeJsonString(d->nics[i].ipv4Address, eV4, 128);
        EscapeJsonString(d->nics[i].ipv6Address, eV6, 128);
        EscapeJsonString(d->nics[i].description, eDesc, MAX_BUFFER * 2);
        wprintf(
            L"    {\n"
            L"      \"Name\": \"%ls\",\n"
            L"      \"Description\": \"%ls\",\n"
            L"      \"MAC\": \"%ls\",\n"
            L"      \"IPv4\": \"%ls\",\n"
            L"      \"IPv6\": \"%ls\",\n"
            L"      \"Up\": %ls,\n"
            L"      \"DHCP\": %ls\n"
            L"    }%ls\n",
            eName, eDesc, eMac, eV4, eV6,
            d->nics[i].isUp ? L"true" : L"false",
            d->nics[i].dhcpEnabled ? L"true" : L"false",
            (i + 1 < d->nicCount) ? L"," : L""
        );
    }
    wprintf(L"  ]\n}\n");
}

static void JsonFormatter_Destroy(IOutputFormatter* this) {
    if (this) free(this);
}

static const IOutputFormatterVtbl JsonVtbl = {
    NULL,
    JsonFormatter_RenderComplete,
    JsonFormatter_Destroy
};

IOutputFormatter* JsonFormatter_Create(bool noColor, bool verbose) {
    JsonFormatter* f = (JsonFormatter*)calloc(1, sizeof(JsonFormatter));
    if (!f) return NULL;
    f->base.vptr = &JsonVtbl;
    f->base.noColor = noColor;
    f->base.verbose = verbose;
    return (IOutputFormatter*)f;
}

/* ---------------- CSV Formatter ---------------- */
typedef struct CsvFormatter {
    IOutputFormatter base;
} CsvFormatter;

static void CsvFormatter_RenderComplete(IOutputFormatter* this, const SystemData* d, bool tpmOnly) {
    (void)this;
    wchar_t eHost[MAX_BUFFER * 2], eVer[64], eStat[MAX_BUFFER * 2];
    EscapeJsonString(d->hostName, eHost, MAX_BUFFER * 2);
    EscapeJsonString(d->tpmVersion, eVer, 64);
    EscapeJsonString(d->tpmStatus, eStat, MAX_BUFFER * 2);

    if (tpmOnly) {
        wprintf(L"\"HostName\",\"TPM_Present\",\"TPM_Version\",\"TPM_Status\"\n");
        wprintf(L"\"%ls\",\"%ls\",\"%ls\",\"%ls\"\n",
            eHost, d->tpmPresent ? L"true" : L"false", eVer, eStat);
        return;
    }

    wchar_t eOsName[MAX_BUFFER * 2], eOsVer[MAX_BUFFER * 2], eSysModel[MAX_BUFFER * 2], eProc[MAX_BUFFER * 2];
    EscapeJsonString(d->osName, eOsName, MAX_BUFFER * 2);
    EscapeJsonString(d->osVersion, eOsVer, MAX_BUFFER * 2);
    EscapeJsonString(d->sysModel, eSysModel, MAX_BUFFER * 2);
    EscapeJsonString(d->processor, eProc, MAX_BUFFER * 2);

    wprintf(L"\"HostName\",\"OSName\",\"OSVersion\",\"SystemModel\",\"Processor\",\"TotalPhysMB\",\"TPM_Version\",\"TPM_Status\"\n");
    wprintf(L"\"%ls\",\"%ls\",\"%ls\",\"%ls\",\"%ls\",%llu,\"%ls\",\"%ls\"\n",
        eHost, eOsName, eOsVer, eSysModel, eProc,
        d->totalPhysicalMemoryMB, eVer, eStat);

    /* One row per NIC */
    wprintf(L"\n\"NIC\",\"MAC\",\"IPv4\",\"IPv6\",\"Up\",\"DHCP\"\n");
    for (size_t i = 0; i < d->nicCount; ++i) {
        wchar_t eName[MAX_BUFFER * 2], eMac[64], eV4[128], eV6[128];
        EscapeJsonString(d->nics[i].name, eName, MAX_BUFFER * 2);
        EscapeJsonString(d->nics[i].macAddress, eMac, 64);
        EscapeJsonString(d->nics[i].ipv4Address, eV4, 128);
        EscapeJsonString(d->nics[i].ipv6Address, eV6, 128);
        wprintf(L"\"%ls\",\"%ls\",\"%ls\",\"%ls\",\"%ls\",\"%ls\"\n",
            eName, eMac, eV4, eV6,
            d->nics[i].isUp ? L"true" : L"false",
            d->nics[i].dhcpEnabled ? L"true" : L"false");
    }
}

static void CsvFormatter_Destroy(IOutputFormatter* this) {
    if (this) free(this);
}

static const IOutputFormatterVtbl CsvVtbl = {
    NULL,
    CsvFormatter_RenderComplete,
    CsvFormatter_Destroy
};

IOutputFormatter* CsvFormatter_Create(bool noColor, bool verbose) {
    CsvFormatter* f = (CsvFormatter*)calloc(1, sizeof(CsvFormatter));
    if (!f) return NULL;
    f->base.vptr = &CsvVtbl;
    f->base.noColor = noColor;
    f->base.verbose = verbose;
    return (IOutputFormatter*)f;
}

/* ========================================================================= */
/* 5. APPLICATION CONTROLLER (ORCHESTRATOR)                                  */
/* ========================================================================= */

typedef struct Application {
    CliOptions* options;
    SystemData* model;
    SystemCollector* collector;
    IOutputFormatter* formatter;

    /* Methods */
    int (*Run)(struct Application* this, int argc, char** argv);
} Application;

static bool EnableVirtualTerminalProcessing(DWORD* outOrigMode) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE || hOut == NULL) return false;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return false; /* not a console (redirected) */
    if (outOrigMode) *outOrigMode = dwMode;
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    return true;
}

static void RestoreConsoleMode(bool wasConsole, DWORD origMode) {
    if (!wasConsole) return;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && hOut != NULL) {
        SetConsoleMode(hOut, origMode);
    }
}

static int Application_Run(Application* this, int argc, char** argv) {
    if (!this->options->Parse(this->options, argc, argv)) {
        return EXIT_FAILURE;
    }

    if (this->options->showHelp) {
        this->options->PrintUsage(this->options);
        return EXIT_SUCCESS;
    }

    if (this->options->showVersion) {
        wprintf(L"machinfo version %hs \n", MACHINFO_VERSION);
        wprintf(L"Copyright (C) 2026 PC/OpenSystems LLC. \n");
        return EXIT_SUCCESS;
    }

    /* Instantiate Polymorphic Formatter */
    switch (this->options->format) {
        case FORMAT_JSON:
            this->formatter = JsonFormatter_Create(this->options->noColor, this->options->verbose);
            break;
        case FORMAT_CSV:
            this->formatter = CsvFormatter_Create(this->options->noColor, this->options->verbose);
            break;
        case FORMAT_TABLE:
        default:
            this->formatter = TableFormatter_Create(this->options->noColor, this->options->verbose);
            break;
    }

    if (!this->formatter) {
        fprintf(stderr, "Fatal: Failed to allocate output formatter.\n");
        return EXIT_FAILURE;
    }

    /* Only table output uses color; JSON/CSV never emit ANSI. */
    DWORD origMode = 0;
    bool isConsole = false;
    if (!this->options->noColor && this->options->format == FORMAT_TABLE) {
        isConsole = EnableVirtualTerminalProcessing(&origMode);
        if (!isConsole) {
            /* Redirected to a file/pipe: suppress color so no ANSI leaks */
            this->options->noColor = true;
            this->formatter->noColor = true;
        }
    }

    /* Data Acquisition */
    if (this->options->showTpmOnly) {
        this->collector->CollectTpmOnly(this->collector);
    } else {
        this->collector->CollectAll(this->collector);
    }

    /* Render Presentation */
    this->formatter->vptr->RenderComplete(this->formatter, this->model, this->options->showTpmOnly);

    RestoreConsoleMode(isConsole, origMode);

    return EXIT_SUCCESS;
}

Application* Application_Create(void) {
    Application* app = (Application*)calloc(1, sizeof(Application));
    if (!app) return NULL;

    app->options = CliOptions_Create();
    app->model = SystemData_Create();
    app->collector = SystemCollector_Create(app->model);
    app->Run = Application_Run;

    return app;
}

void Application_Destroy(Application* this) {
    if (!this) return;
    if (this->formatter) this->formatter->vptr->Destroy(this->formatter);
    if (this->collector) SystemCollector_Destroy(this->collector);
    if (this->model) SystemData_Destroy(this->model);
    if (this->options) CliOptions_Destroy(this->options);
    free(this);
}

/* ========================================================================= */
/* 6. ENTRY POINT                                                            */
/* ========================================================================= */

int main(int argc, char** argv) {
    Application* app = Application_Create();
    if (!app) {
        fprintf(stderr, "Fatal: Out of memory during system initialization.\n");
        return EXIT_FAILURE;
    }

    int exitCode = app->Run(app, argc, argv);
    Application_Destroy(app);

    return exitCode;
}
