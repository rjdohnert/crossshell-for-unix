// Copyright (c) 2026,  CrossShell Developers
// licensed under the BSD-3-Clause License

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#define NETCONF_VERSION "2.3.0"

typedef struct {
    char port_id[16];       /* HP-UX port label: lan0, lan1... */
    char name[128];         /* Adapter Friendly Name */
    char desc[256];         /* Hardware Description */
    char type[20];          /* ETHERNET, WIRELESS, VIRTUAL, LOOPBACK, BLUETOOTH */
    char link_state[12];    /* UP, DOWN */
    char speed_duplex[16];  /* 10000FD, 1000FD, 100FD, AUTO */
    char config_mode[24];   /* LACP_AUTO, MANUAL, VIRT_SWITCH */
    char group_cap[16];     /* Group Capability / Aggregation Key */
    char priority[8];       /* Port Priority */
    char station_addr[32];  /* 0x00155D... */
    char ip_addrs[256];     /* Display IP */
    char ipv4[128];         /* Primary IPv4 address with prefix */
    char ipv6[128];         /* Primary IPv6 address with prefix */
    char gateway[128];      /* Default Gateway */
    char dns[128];          /* Primary DNS Server(s) */
    unsigned int mtu;       /* MTU */
    bool is_up;
} NetConfEntry;

static int g_count = 0;
static int g_capacity = 0;
static NetConfEntry *g_entries = NULL;

static void free_entries(void) {
    if (g_entries) {
        free(g_entries);
        g_entries = NULL;
    }
    g_count = 0;
    g_capacity = 0;
}

static bool add_entry(const NetConfEntry *e) {
    if (g_count >= g_capacity) {
        int new_cap = (g_capacity == 0) ? 32 : (g_capacity * 2);
        NetConfEntry *new_entries = (NetConfEntry *)realloc(g_entries, new_cap * sizeof(NetConfEntry));
        if (!new_entries) return false;
        g_entries = new_entries;
        g_capacity = new_cap;
    }
    g_entries[g_count++] = *e;
    return true;
}

static bool str_case_contains(const char *haystack, const char *needle) {
    if (!haystack || !needle) return false;
    if (!*needle) return true;
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && (tolower((unsigned char)*h) == tolower((unsigned char)*n))) {
            h++;
            n++;
        }
        if (!*n) return true;
    }
    return false;
}

static bool str_case_equals(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

void wchar_to_utf8(const WCHAR *wstr, char *out, size_t out_size) {
    if (!wstr || !out || out_size == 0) return;
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out, (int)out_size, NULL, NULL);
}

/* Format speed using HP-UX portconf notation (e.g. 1000FD, 10000FD, AUTO) */
void format_portconf_speed(ULONG64 bps, char *out, size_t out_size) {
    if (bps == 0 || bps == (ULONG64)-1) {
        strncpy_s(out, out_size, "AUTO", _TRUNCATE);
    } else if (bps >= 10000000000ULL) {
        snprintf(out, out_size, "%llu000FD", bps / 10000000000ULL);
    } else if (bps >= 1000000000ULL) {
        strncpy_s(out, out_size, "1000FD", _TRUNCATE);
    } else if (bps >= 100000000ULL) {
        strncpy_s(out, out_size, "100FD", _TRUNCATE);
    } else {
        snprintf(out, out_size, "%lluM", bps / 1000000ULL);
    }
}

/* Identify adapter class and portconf operational modes */
void inspect_adapter(PIP_ADAPTER_ADDRESSES pAdapter, NetConfEntry *e) {
    bool is_virt = (wcsstr(pAdapter->Description, L"Virtual") != NULL || 
                    wcsstr(pAdapter->Description, L"Hyper-V") != NULL || 
                    wcsstr(pAdapter->Description, L"TAP") != NULL || 
                    wcsstr(pAdapter->Description, L"VPN") != NULL ||
                    wcsstr(pAdapter->FriendlyName, L"vEthernet") != NULL ||
                    pAdapter->IfType == IF_TYPE_TUNNEL);

    bool is_wifi = (pAdapter->IfType == IF_TYPE_IEEE80211);
    bool is_loopback = (pAdapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK);
    bool is_bluetooth = (wcsstr(pAdapter->Description, L"Bluetooth") != NULL ||
                         wcsstr(pAdapter->FriendlyName, L"Bluetooth") != NULL);

    if (is_loopback) {
        strncpy_s(e->type, sizeof(e->type), "LOOPBACK", _TRUNCATE);
        strncpy_s(e->config_mode, sizeof(e->config_mode), "MANUAL", _TRUNCATE);
        strncpy_s(e->group_cap, sizeof(e->group_cap), "NONE", _TRUNCATE);
        strncpy_s(e->priority, sizeof(e->priority), "0", _TRUNCATE);
    } else if (is_bluetooth) {
        strncpy_s(e->type, sizeof(e->type), "BLUETOOTH", _TRUNCATE);
        strncpy_s(e->config_mode, sizeof(e->config_mode), "MANUAL", _TRUNCATE);
        strncpy_s(e->group_cap, sizeof(e->group_cap), "NONE", _TRUNCATE);
        strncpy_s(e->priority, sizeof(e->priority), "0", _TRUNCATE);
    } else if (is_virt) {
        strncpy_s(e->type, sizeof(e->type), "VIRTUAL", _TRUNCATE);
        strncpy_s(e->config_mode, sizeof(e->config_mode), "VIRT_SWITCH", _TRUNCATE);
        strncpy_s(e->group_cap, sizeof(e->group_cap), "VSWITCH", _TRUNCATE);
        strncpy_s(e->priority, sizeof(e->priority), "128", _TRUNCATE);
    } else if (is_wifi) {
        strncpy_s(e->type, sizeof(e->type), "WIRELESS", _TRUNCATE);
        strncpy_s(e->config_mode, sizeof(e->config_mode), "MANUAL", _TRUNCATE);
        strncpy_s(e->group_cap, sizeof(e->group_cap), "NONE", _TRUNCATE);
        strncpy_s(e->priority, sizeof(e->priority), "0", _TRUNCATE);
    } else {
        strncpy_s(e->type, sizeof(e->type), "ETHERNET", _TRUNCATE);
        strncpy_s(e->config_mode, sizeof(e->config_mode), "LACP_AUTO", _TRUNCATE);
        strncpy_s(e->group_cap, sizeof(e->group_cap), "5", _TRUNCATE);
        strncpy_s(e->priority, sizeof(e->priority), "0", _TRUNCATE);
    }
}

bool collect_interfaces(bool include_all, bool include_filters, bool prefer_ipv6, const char *target_filter) {
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS;
    if (include_filters) {
        flags |= GAA_FLAG_INCLUDE_ALL_INTERFACES;
    }

    ULONG outBufLen = 16384;
    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    DWORD ret = 0;

    for (int i = 0; i < 3; i++) {
        pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
        if (!pAddresses) return false;
        ret = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, pAddresses, &outBufLen);
        if (ret == ERROR_BUFFER_OVERFLOW) {
            free(pAddresses);
            pAddresses = NULL;
        } else {
            break;
        }
    }

    if (ret != NO_ERROR) {
        if (pAddresses) free(pAddresses);
        return false;
    }

    int lan_idx = 0;
    PIP_ADAPTER_ADDRESSES pCurr = pAddresses;
    while (pCurr) {
        bool is_up = (pCurr->OperStatus == IfOperStatusUp);
        if (!include_all && !is_up) {
            pCurr = pCurr->Next;
            continue;
        }

        NetConfEntry e;
        memset(&e, 0, sizeof(e));
        snprintf(e.port_id, sizeof(e.port_id), "lan%d", lan_idx++);
        e.is_up = is_up;
        e.mtu = pCurr->Mtu;

        wchar_to_utf8(pCurr->FriendlyName, e.name, sizeof(e.name));
        wchar_to_utf8(pCurr->Description, e.desc, sizeof(e.desc));
        strncpy_s(e.link_state, sizeof(e.link_state), is_up ? "UP" : "DOWN", _TRUNCATE);

        format_portconf_speed(pCurr->TransmitLinkSpeed, e.speed_duplex, sizeof(e.speed_duplex));
        inspect_adapter(pCurr, &e);

        // Hardware Station Address (MAC)
        if (pCurr->PhysicalAddressLength == 6) {
            snprintf(e.station_addr, sizeof(e.station_addr), "0x%02X%02X%02X%02X%02X%02X",
                     pCurr->PhysicalAddress[0], pCurr->PhysicalAddress[1],
                     pCurr->PhysicalAddress[2], pCurr->PhysicalAddress[3],
                     pCurr->PhysicalAddress[4], pCurr->PhysicalAddress[5]);
        } else {
            strncpy_s(e.station_addr, sizeof(e.station_addr), "0x000000000000", _TRUNCATE);
        }

        // IP address resolution (IPv4 and IPv6)
        PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurr->FirstUnicastAddress;
        while (pUnicast) {
            if (pUnicast->Address.lpSockaddr->sa_family == AF_INET && e.ipv4[0] == '\0') {
                char ip_buf[INET_ADDRSTRLEN] = {0};
                struct sockaddr_in *sa = (struct sockaddr_in *)pUnicast->Address.lpSockaddr;
                inet_ntop(AF_INET, &(sa->sin_addr), ip_buf, sizeof(ip_buf));
                snprintf(e.ipv4, sizeof(e.ipv4), "%s/%u", ip_buf, pUnicast->OnLinkPrefixLength);
            } else if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6 && e.ipv6[0] == '\0') {
                char ip6_buf[INET6_ADDRSTRLEN] = {0};
                struct sockaddr_in6 *sa = (struct sockaddr_in6 *)pUnicast->Address.lpSockaddr;
                inet_ntop(AF_INET6, &(sa->sin6_addr), ip6_buf, sizeof(ip6_buf));
                snprintf(e.ipv6, sizeof(e.ipv6), "%s/%u", ip6_buf, pUnicast->OnLinkPrefixLength);
            }
            pUnicast = pUnicast->Next;
        }

        // Set primary display IP
        if (prefer_ipv6) {
            if (e.ipv6[0] != '\0') {
                strncpy_s(e.ip_addrs, sizeof(e.ip_addrs), e.ipv6, _TRUNCATE);
            } else if (e.ipv4[0] != '\0') {
                strncpy_s(e.ip_addrs, sizeof(e.ip_addrs), e.ipv4, _TRUNCATE);
            } else {
                strncpy_s(e.ip_addrs, sizeof(e.ip_addrs), "UNASSIGNED", _TRUNCATE);
            }
        } else {
            if (e.ipv4[0] != '\0') {
                strncpy_s(e.ip_addrs, sizeof(e.ip_addrs), e.ipv4, _TRUNCATE);
            } else if (e.ipv6[0] != '\0') {
                strncpy_s(e.ip_addrs, sizeof(e.ip_addrs), e.ipv6, _TRUNCATE);
            } else {
                strncpy_s(e.ip_addrs, sizeof(e.ip_addrs), "UNASSIGNED", _TRUNCATE);
            }
        }

        // Gateway resolution
        char gw_ipv4[64] = {0};
        char gw_ipv6[64] = {0};
        PIP_ADAPTER_GATEWAY_ADDRESS_LH pGateway = pCurr->FirstGatewayAddress;
        while (pGateway) {
            if (pGateway->Address.lpSockaddr->sa_family == AF_INET && gw_ipv4[0] == '\0') {
                struct sockaddr_in *sa = (struct sockaddr_in *)pGateway->Address.lpSockaddr;
                inet_ntop(AF_INET, &(sa->sin_addr), gw_ipv4, sizeof(gw_ipv4));
            } else if (pGateway->Address.lpSockaddr->sa_family == AF_INET6 && gw_ipv6[0] == '\0') {
                struct sockaddr_in6 *sa = (struct sockaddr_in6 *)pGateway->Address.lpSockaddr;
                inet_ntop(AF_INET6, &(sa->sin6_addr), gw_ipv6, sizeof(gw_ipv6));
            }
            pGateway = pGateway->Next;
        }
        if (gw_ipv4[0] != '\0') {
            strncpy_s(e.gateway, sizeof(e.gateway), gw_ipv4, _TRUNCATE);
        } else if (gw_ipv6[0] != '\0') {
            strncpy_s(e.gateway, sizeof(e.gateway), gw_ipv6, _TRUNCATE);
        }

        // DNS resolution
        PIP_ADAPTER_DNS_SERVER_ADDRESS pDns = pCurr->FirstDnsServerAddress;
        while (pDns) {
            char dns_buf[INET6_ADDRSTRLEN] = {0};
            if (pDns->Address.lpSockaddr->sa_family == AF_INET) {
                struct sockaddr_in *sa = (struct sockaddr_in *)pDns->Address.lpSockaddr;
                inet_ntop(AF_INET, &(sa->sin_addr), dns_buf, sizeof(dns_buf));
            } else if (pDns->Address.lpSockaddr->sa_family == AF_INET6) {
                struct sockaddr_in6 *sa = (struct sockaddr_in6 *)pDns->Address.lpSockaddr;
                inet_ntop(AF_INET6, &(sa->sin6_addr), dns_buf, sizeof(dns_buf));
            }

            if (dns_buf[0] != '\0') {
                if (e.dns[0] == '\0') {
                    strncpy_s(e.dns, sizeof(e.dns), dns_buf, _TRUNCATE);
                } else if (!strstr(e.dns, dns_buf)) {
                    strncat_s(e.dns, sizeof(e.dns), ", ", _TRUNCATE);
                    strncat_s(e.dns, sizeof(e.dns), dns_buf, _TRUNCATE);
                    break;
                }
            }
            pDns = pDns->Next;
        }

        // Target interface filter matching
        if (target_filter && *target_filter) {
            if (!str_case_equals(e.port_id, target_filter) &&
                !str_case_contains(e.name, target_filter) &&
                !str_case_contains(e.desc, target_filter)) {
                pCurr = pCurr->Next;
                continue;
            }
        }

        add_entry(&e);
        pCurr = pCurr->Next;
    }

    if (pAddresses) free(pAddresses);
    return true;
}

static void json_escape_print(const char *str) {
    if (!str) return;
    for (const char *p = str; *p; p++) {
        if (*p == '"') printf("\\\"");
        else if (*p == '\\') printf("\\\\");
        else if (*p == '\b') printf("\\b");
        else if (*p == '\f') printf("\\f");
        else if (*p == '\n') printf("\\n");
        else if (*p == '\r') printf("\\r");
        else if (*p == '\t') printf("\\t");
        else putchar(*p);
    }
}

static void csv_escape_print(const char *str) {
    if (!str) return;
    for (const char *p = str; *p; p++) {
        if (*p == '"') {
            putchar('"');
            putchar('"');
        } else {
            putchar(*p);
        }
    }
}

/* Portconf-styled table output */
void render_table(bool detailed) {
    printf("\nNetwork Port Configuration (netconf v%s)\n", NETCONF_VERSION);
    printf("========================================================================================================================\n");
    printf("%-7s %-10s %-6s %-10s %-12s %-10s %-6s %-16s %-20s %s\n",
           "PORT", "TYPE", "LINK", "SPEED/DUP", "CONFIG_MODE", "GROUP_CAP", "PRI", "STATION_ADDR", "IP-ADDRESS", "NAME");
    printf("------------------------------------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < g_count; i++) {
        NetConfEntry *e = &g_entries[i];
        printf("%-7s %-10s %-6s %-10s %-12s %-10s %-6s %-16s %-20s %s\n",
               e->port_id, e->type, e->link_state, e->speed_duplex, e->config_mode,
               e->group_cap, e->priority, e->station_addr, e->ip_addrs, e->name);
        if (detailed) {
            printf("        Hardware: %s\n", e->desc[0] ? e->desc : "N/A");
            printf("        IPv4: %-18s  IPv6: %s\n",
                   e->ipv4[0] ? e->ipv4 : "UNASSIGNED",
                   e->ipv6[0] ? e->ipv6 : "UNASSIGNED");
            printf("        Gateway: %-15s  DNS: %-20s  MTU: %u\n",
                   e->gateway[0] ? e->gateway : "NONE",
                   e->dns[0] ? e->dns : "NONE",
                   e->mtu);
            if (i < g_count - 1) {
                printf("  --------------------------------------------------------------------------------------------------------------------\n");
            }
        }
    }
    printf("========================================================================================================================\n\n");
}

/* JSON Output */
void render_json(void) {
    printf("[\n");
    for (int i = 0; i < g_count; i++) {
        NetConfEntry *e = &g_entries[i];
        printf("  {\n");
        printf("    \"port\": \"%s\",\n", e->port_id);
        printf("    \"type\": \"%s\",\n", e->type);
        printf("    \"link_state\": \"%s\",\n", e->link_state);
        printf("    \"speed_duplex\": \"%s\",\n", e->speed_duplex);
        printf("    \"config_mode\": \"%s\",\n", e->config_mode);
        printf("    \"group_capability\": \"%s\",\n", e->group_cap);
        printf("    \"priority\": \"%s\",\n", e->priority);
        printf("    \"station_address\": \"%s\",\n", e->station_addr);
        printf("    \"ip_address\": \"%s\",\n", e->ip_addrs);
        printf("    \"ipv4\": \"%s\",\n", e->ipv4[0] ? e->ipv4 : "UNASSIGNED");
        printf("    \"ipv6\": \"%s\",\n", e->ipv6[0] ? e->ipv6 : "UNASSIGNED");
        printf("    \"gateway\": \"%s\",\n", e->gateway[0] ? e->gateway : "NONE");
        printf("    \"dns\": \""); json_escape_print(e->dns[0] ? e->dns : "NONE"); printf("\",\n");
        printf("    \"mtu\": %u,\n", e->mtu);
        printf("    \"name\": \""); json_escape_print(e->name); printf("\",\n");
        printf("    \"description\": \""); json_escape_print(e->desc); printf("\"\n");
        printf("  }%s\n", (i == g_count - 1) ? "" : ",");
    }
    printf("]\n");
}

/* CSV Output */
void render_csv(void) {
    printf("port,type,link_state,speed_duplex,config_mode,group_capability,priority,station_address,ip_address,ipv4,ipv6,gateway,dns,mtu,name,description\n");
    for (int i = 0; i < g_count; i++) {
        NetConfEntry *e = &g_entries[i];
        printf("\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"",
               e->port_id, e->type, e->link_state, e->speed_duplex, e->config_mode,
               e->group_cap, e->priority, e->station_addr, e->ip_addrs,
               e->ipv4[0] ? e->ipv4 : "UNASSIGNED",
               e->ipv6[0] ? e->ipv6 : "UNASSIGNED",
               e->gateway[0] ? e->gateway : "NONE");
        csv_escape_print(e->dns[0] ? e->dns : "NONE");
        printf("\",%u,\"", e->mtu);
        csv_escape_print(e->name);
        printf("\",\"");
        csv_escape_print(e->desc);
        printf("\"\n");
    }
}

/* Comprehensive Help */
void render_help(const char *prog) {
    printf("NAME\n");
    printf("    netconf - report Ethernet, Wireless, and Virtual ports\n\n");
    printf("SYNOPSIS\n");
    printf("    %s [OPTIONS] [INTERFACE]\n\n", prog);
    printf("DESCRIPTION\n");
    printf("    netconf is a native Win32/NT network reporting utility. It enumerates\n");
    printf("    Ethernet, IEEE 802.11 Wireless, and Virtual interfaces (WSL, Hyper-V,\n");
    printf("    TAP, and VPN), displaying operational link states, port duplex notations\n");
    printf("    (1000FD, AUTO), APA config modes (LACP_AUTO, VIRT_SWITCH, MANUAL), group\n");
    printf("    capabilities, hardware station addresses, assigned IP addresses, default\n");
    printf("    gateways, DNS servers, and MTU.\n\n");
    printf("OPTIONS\n");
    printf("    -h, --help        Display this manual and exit.\n");
    printf("    -j, --json        Output records in JSON format (pipable to jq or PowerShell).\n");
    printf("    -c, --csv         Output records in Comma-Separated Values (CSV).\n");
    printf("    -d, --detail      Display detailed adapter telemetry (IPv4, IPv6, Gateway, DNS, MTU).\n");
    printf("    -a, --all         Report all interfaces (including unlinked DOWN ports).\n");
    printf("    -6, --ipv6        Prefer IPv6 addresses in the main IP-ADDRESS column.\n");
    printf("    --filters         Include intermediate NDIS lightweight filter (LWF) pseudo-devices.\n");
    printf("    -v, --version     Print program version.\n\n");
    printf("ARGUMENTS\n");
    printf("    INTERFACE         Optional port ID (e.g. lan0) or adapter name substring (e.g. Wi-Fi).\n\n");
    printf("PIPELINE & FILTER EXAMPLES\n");
    printf("    # Inspect only port lan0 in detail:\n");
    printf("    %s -d lan0\n\n", prog);
    printf("    # Filter only Wi-Fi adapter:\n");
    printf("    %s Wi-Fi\n\n", prog);
    printf("    # Parse JSON directly in PowerShell:\n");
    printf("    %s -j | ConvertFrom-Json | Select-Object port, type, speed_duplex, station_address, gateway\n\n", prog);
    printf("    # Export CSV for network inventory:\n");
    printf("    %s --csv > network_ports.csv\n\n", prog);
}

int main(int argc, char *argv[]) {
    // Disable standard I/O buffer to allow immediate pipe delivery
    setvbuf(stdout, NULL, _IONBF, 0);

    bool opt_json = false;
    bool opt_csv = false;
    bool opt_all = false;
    bool opt_detail = false;
    bool opt_ipv6 = false;
    bool opt_filters = false;
    const char *target_filter = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "/?") == 0) {
            render_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-j") == 0 || strcmp(argv[i], "--json") == 0) {
            opt_json = true;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--csv") == 0) {
            opt_csv = true;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--detail") == 0) {
            opt_detail = true;
        } else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0) {
            opt_all = true;
        } else if (strcmp(argv[i], "-6") == 0 || strcmp(argv[i], "--ipv6") == 0) {
            opt_ipv6 = true;
        } else if (strcmp(argv[i], "--filters") == 0) {
            opt_filters = true;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("netconf version %s \n", NETCONF_VERSION);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "netconf: unrecognized option '%s'\nTry 'netconf --help' for details.\n", argv[i]);
            return 1;
        } else {
            if (!target_filter) {
                target_filter = argv[i];
            } else {
                fprintf(stderr, "netconf: too many arguments specified ('%s')\n", argv[i]);
                return 1;
            }
        }
    }

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    if (!collect_interfaces(opt_all, opt_filters, opt_ipv6, target_filter)) {
        fprintf(stderr, "netconf: failed querying Win32 network subsystem.\n");
        WSACleanup();
        free_entries();
        return 1;
    }

    if (target_filter && g_count == 0) {
        fprintf(stderr, "netconf: no network interfaces matched '%s'\n", target_filter);
        WSACleanup();
        free_entries();
        return 1;
    }

    if (opt_json) {
        render_json();
    } else if (opt_csv) {
        render_csv();
    } else {
        render_table(opt_detail);
    }

    WSACleanup();
    free_entries();
    return 0;
}
