#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#pragma comment(lib, "advapi32.lib")

/* NTSTATUS and RTL_OSVERSIONINFOW are hidden under WIN32_LEAN_AND_MEAN */
#ifndef NTSTATUS
typedef LONG NTSTATUS;
#endif

#define UNAME_VERSION "2.0.0"

/* Function pointer definition for RtlGetVersion from NTDLL */
typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

typedef struct {
    char sysname[64];      /* -s: Kernel name (e.g., Windows_NT) */
    char nodename[256];    /* -n: Network node hostname */
    char release[64];      /* -r: Kernel release (e.g., 10.0.22631) */
    char version[256];     /* -v: Kernel version & build details */
    char machine[32];      /* -m: Machine hardware architecture */
    char processor[32];    /* -p: Processor type */
    char platform[32];     /* -i: Hardware platform */
    char os_name[128];     /* -o: Operating system marketing name */
} UnameInfo;

/* Fetch registry string helper */
static void get_reg_string(HKEY root, const char* subkey, const char* value_name, char* dest, DWORD dest_size) {
    HKEY hKey;
    if (RegOpenKeyExA(root, subkey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0;
        DWORD size = dest_size;
        if (RegQueryValueExA(hKey, value_name, NULL, &type, (LPBYTE)dest, &size) == ERROR_SUCCESS) {
            dest[dest_size - 1] = '\0';
        }
        RegCloseKey(hKey);
    }
}

/* Fetch registry DWORD helper */
static DWORD get_reg_dword(HKEY root, const char* subkey, const char* value_name) {
    HKEY hKey;
    DWORD result = 0;
    if (RegOpenKeyExA(root, subkey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD);
        DWORD type = 0;
        RegQueryValueExA(hKey, value_name, NULL, &type, (LPBYTE)&result, &size);
        RegCloseKey(hKey);
    }
    return result;
}

static void gather_system_info(UnameInfo* info) {
    memset(info, 0, sizeof(UnameInfo));

    /* 1. Kernel Name (-s) */
    snprintf(info->sysname, sizeof(info->sysname), "%s", "Windows NT");

    /* 2. Nodename (-n) */
    DWORD host_len = sizeof(info->nodename);
    if (!GetComputerNameExA(ComputerNameDnsHostname, info->nodename, &host_len)) {
        snprintf(info->nodename, sizeof(info->nodename), "%s", "localhost");
    }

    /* 3. Query true Kernel Version via RtlGetVersion */
    RTL_OSVERSIONINFOW osvi;
    memset(&osvi, 0, sizeof(osvi));
    osvi.dwOSVersionInfoSize = sizeof(osvi);

    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionPtr pRtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");
    if (pRtlGetVersion) {
        pRtlGetVersion(&osvi);
    }

    /* Retrieve Extended Build Information from Registry */
    const char* cv_key = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    char prod_name[128] = { 0 };
    char display_ver[32] = { 0 };
    get_reg_string(HKEY_LOCAL_MACHINE, cv_key, "ProductName", prod_name, sizeof(prod_name));
    get_reg_string(HKEY_LOCAL_MACHINE, cv_key, "DisplayVersion", display_ver, sizeof(display_ver));
    DWORD ubr = get_reg_dword(HKEY_LOCAL_MACHINE, cv_key, "UBR");

    /* 4. Kernel Release (-r): Major.Minor.Build */
    snprintf(info->release, sizeof(info->release), "%lu.%lu.%lu",
        osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);

    /* 5. Kernel Version (-v): Build number and revision */
    if (ubr > 0) {
        snprintf(info->version, sizeof(info->version), "Build %lu.%lu",
            osvi.dwBuildNumber, ubr);
    } else {
        snprintf(info->version, sizeof(info->version), "Build %lu",
            osvi.dwBuildNumber);
    }

    /* 6. Hardware Architecture (-m, -p, -i) */
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);

    switch (si.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
        snprintf(info->machine, sizeof(info->machine), "%s", "x86_64");
        snprintf(info->processor, sizeof(info->processor), "%s", "x86_64");
        snprintf(info->platform, sizeof(info->platform), "%s", "PC");
        break;
    case PROCESSOR_ARCHITECTURE_ARM64:
        snprintf(info->machine, sizeof(info->machine), "%s", "aarch64");
        snprintf(info->processor, sizeof(info->processor), "%s", "arm64");
        snprintf(info->platform, sizeof(info->platform), "%s", "ARM");
        break;
    case PROCESSOR_ARCHITECTURE_INTEL:
        snprintf(info->machine, sizeof(info->machine), "%s", "i686");
        snprintf(info->processor, sizeof(info->processor), "%s", "i686");
        snprintf(info->platform, sizeof(info->platform), "%s", "PC");
        break;
    case PROCESSOR_ARCHITECTURE_ARM:
        snprintf(info->machine, sizeof(info->machine), "%s", "armv7l");
        snprintf(info->processor, sizeof(info->processor), "%s", "arm");
        snprintf(info->platform, sizeof(info->platform), "%s", "ARM");
        break;
    default:
        snprintf(info->machine, sizeof(info->machine), "%s", "unknown");
        snprintf(info->processor, sizeof(info->processor), "%s", "unknown");
        snprintf(info->platform, sizeof(info->platform), "%s", "unknown");
        break;
    }

    /* 7. Operating System (-o) */
    if (prod_name[0] != '\0') {
        if (display_ver[0] != '\0') {
            snprintf(info->os_name, sizeof(info->os_name), "%s %s", prod_name, display_ver);
        } else {
            snprintf(info->os_name, sizeof(info->os_name), "%s", prod_name);
        }
    } else {
        snprintf(info->os_name, sizeof(info->os_name), "%s", "Microsoft Windows");
    }
}

static void print_help(const char* prog) {
    printf("Usage: %s [OPTION]...\n", prog);
    printf("Print certain system information. With no OPTION, same as -s.\n\n");
    printf("  -a, --all                print all information, in the following order,\n");
    printf("                             except omit -p and -i if unknown:\n");
    printf("  -s, --kernel-name        print the kernel name\n");
    printf("  -n, --nodename           print the network node hostname\n");
    printf("  -r, --kernel-release     print the kernel release\n");
    printf("  -v, --kernel-version     print the kernel version and build number\n");
    printf("  -m, --machine            print the machine hardware name\n");
    printf("  -p, --processor          print the processor type\n");
    printf("  -i, --hardware-platform  print the hardware platform\n");
    printf("  -o, --operating-system   print the operating system\n");
    printf("      --help     display this help and exit\n");
    printf("      --version  output version information and exit\n");
}

int main(int argc, char* argv[]) {
    bool flag_s = false, flag_n = false, flag_r = false, flag_v = false;
    bool flag_m = false, flag_p = false, flag_i = false, flag_o = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("uname %s\n", UNAME_VERSION);
            return 0;
        } else if (strcmp(argv[i], "--all") == 0) {
            flag_s = flag_n = flag_r = flag_v = flag_m = flag_p = flag_i = flag_o = true;
        } else if (strcmp(argv[i], "--kernel-name") == 0) {
            flag_s = true;
        } else if (strcmp(argv[i], "--nodename") == 0) {
            flag_n = true;
        } else if (strcmp(argv[i], "--kernel-release") == 0) {
            flag_r = true;
        } else if (strcmp(argv[i], "--kernel-version") == 0) {
            flag_v = true;
        } else if (strcmp(argv[i], "--machine") == 0) {
            flag_m = true;
        } else if (strcmp(argv[i], "--processor") == 0) {
            flag_p = true;
        } else if (strcmp(argv[i], "--hardware-platform") == 0) {
            flag_i = true;
        } else if (strcmp(argv[i], "--operating-system") == 0) {
            flag_o = true;
        } else if (argv[i][0] == '-' && argv[i][1] != '-') {
            /* Support bundled flags like -snrvm */
            for (size_t j = 1; j < strlen(argv[i]); j++) {
                switch (argv[i][j]) {
                case 'a': flag_s = flag_n = flag_r = flag_v = flag_m = flag_p = flag_i = flag_o = true; break;
                case 's': flag_s = true; break;
                case 'n': flag_n = true; break;
                case 'r': flag_r = true; break;
                case 'v': flag_v = true; break;
                case 'm': flag_m = true; break;
                case 'p': flag_p = true; break;
                case 'i': flag_i = true; break;
                case 'o': flag_o = true; break;
                default:
                    fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], argv[i][j]);
                    fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                    return 1;
                }
            }
        } else {
            fprintf(stderr, "%s: extra operand '%s'\n", argv[0], argv[i]);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return 1;
        }
    }

    /* Default behavior: with no options specified, act as -s */
    if (!flag_s && !flag_n && !flag_r && !flag_v && !flag_m && !flag_p && !flag_i && !flag_o) {
        flag_s = true;
    }

    UnameInfo info;
    gather_system_info(&info);

    /* Print flags in strict POSIX/GNU canonical output order */
    bool first = true;
    #define PRINT_FIELD(cond, str) \
        if (cond) { \
            if (!first) putchar(' '); \
            fputs(str, stdout); \
            first = false; \
        }

    PRINT_FIELD(flag_s, info.sysname);
    PRINT_FIELD(flag_n, info.nodename);
    PRINT_FIELD(flag_r, info.release);
    PRINT_FIELD(flag_v, info.version);
    PRINT_FIELD(flag_m, info.machine);
    PRINT_FIELD(flag_p, info.processor);
    PRINT_FIELD(flag_i, info.platform);
    PRINT_FIELD(flag_o, info.os_name);

    putchar('\n');
    return 0;
}