/**
 * ============================================================================
 * Program     : wget (CrossShell for UNIX)
 * Copyright (C) 2026, PCX/OpenSystems LLC
 * Licensed under the terms and conditions of the BSD-3 Clause license
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "winhttp.lib")

#define WGET_VERSION "1.1.0"
#define WGET_DEFAULT_USER_AGENT "Wget/1.1.0 (Windows NT; WinHTTP)"
#define BUFFER_SIZE 8192

/* ============================================================================
 * HELPER: String & UTF-16 Conversion Utility
 * ============================================================================ */
static WCHAR* Utf8ToUtf16(const char* utf8Str) {
    if (!utf8Str) return NULL;
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, NULL, 0);
    if (len <= 0) return NULL;
    WCHAR* wstr = (WCHAR*)malloc(sizeof(WCHAR) * len);
    if (wstr) MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, wstr, len);
    return wstr;
}

static char* Utf16ToUtf8(const WCHAR* utf16Str) {
    if (!utf16Str) return NULL;
    int len = WideCharToMultiByte(CP_UTF8, 0, utf16Str, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return NULL;
    char* str = (char*)malloc(sizeof(char) * len);
    if (str) WideCharToMultiByte(CP_UTF8, 0, utf16Str, -1, str, len, NULL, NULL);
    return str;
}

/* Parse a non-negative integer with validation; returns false on garbage. */
static bool ParseNonNegativeInt(const char* s, int* out) {
    if (!s || !*s) return false;
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < 0) return false;
    *out = (int)v;
    return true;
}

/* Parse a download rate like "100k", "2m", or plain bytes/sec. */
static long long ParseRate(const char* s) {
    if (!s || !*s) return 0;
    char* end = NULL;
    long long v = _strtoi64(s, &end, 10);
    if (end == s || v < 0) return 0;
    if (*end == 'k' || *end == 'K') v *= 1024LL;
    else if (*end == 'm' || *end == 'M') v *= 1024LL * 1024LL;
    else if (*end == 'g' || *end == 'G') v *= 1024LL * 1024LL * 1024LL;
    return v;
}

/* ============================================================================
 * CLASS: WgetOptions
 * Handles and encapsulates CLI arguments, switches, and runtime flags.
 * Uses GNU wget qualifier semantics.
 * ============================================================================ */
typedef struct WgetOptions WgetOptions;

struct WgetOptions {
    /* Fields */
    char* url;
    char* method;
    char* output_file;       /* -O <file> / --output-document */
    bool  remote_name;       /* --content-disposition / default naming */
    char* post_data;         /* --post-data */
    char* user_agent;        /* --user-agent */
    char* user;              /* --user */
    char* password;          /* --password */
    bool  follow_redirects;  /* on by default in wget; --max-redirect 0 disables */
    bool  quiet;             /* -q / --quiet */
    bool  verbose;           /* -v / --verbose */
    bool  insecure;          /* --no-check-certificate */
    bool  head_only;         /* --spider / -S --method HEAD */
    bool  timestamping;      /* -N / --timestamping */
    bool  continue_download; /* -c / --continue */
    int   tries;             /* --tries <n> (0 = infinite) */
    int   wait_seconds;      /* --wait <secs> between retries */
    long long limit_rate;    /* --limit-rate <bytes/sec> (0 = unlimited) */
    bool  no_clobber;        /* --no-clobber: skip if file exists */
    char* proxy;             /* --proxy <host:port> (also http_proxy/https_proxy env) */
    char* proxy_user;        /* --proxy-user */
    char* proxy_password;    /* --proxy-password */
    int   timeout_ms;        /* --timeout <secs> -> all stages */
    int   dns_timeout_ms;    /* --dns-timeout */
    int   connect_timeout_ms;/* --connect-timeout */
    int   read_timeout_ms;   /* --read-timeout */

    char** headers;
    int    header_count;
    int    header_capacity;

    /* Methods */
    bool (*Parse)(WgetOptions* this, int argc, char** argv);
    void (*AddHeader)(WgetOptions* this, const char* header);
    void (*PrintHelp)(const WgetOptions* this);
    void (*PrintVersion)(const WgetOptions* this);
};

static void WgetOptions_AddHeader(WgetOptions* this, const char* header) {
    if (this->header_count >= this->header_capacity) {
        this->header_capacity *= 2;
        this->headers = (char**)realloc(this->headers, sizeof(char*) * this->header_capacity);
    }
    this->headers[this->header_count++] = _strdup(header);
}

static void WgetOptions_PrintHelp(const WgetOptions* this) {
    (void)this;
    printf(
        "Usage: wget [options...] <url>\n"
        "Download:\n"
        "  -O, --output-document <file>  Write documents to FILE\n"
        "  -c, --continue                Resume getting a partially-downloaded file\n"
        "  -N, --timestamping            Don't re-retrieve files unless newer than local\n"
        "      --content-disposition     Honor Content-Disposition filename for output\n"
        "  -q, --quiet                   Quiet mode (no output)\n"
        "  -v, --verbose                 Be verbose (default)\n"
        "      --tries <n>               Set number of retries to N (0 = unlimited, up to 20)\n"
        "      --wait <secs>             Wait SECS between retrievals\n"
        "      --limit-rate <rate>       Limit download speed (e.g. 100k, 2m)\n"
        "      --no-clobber              Skip download if file already exists\n"
        "HTTP:\n"
        "      --method <command>        Use HTTP method (GET, POST, HEAD, ...)\n"
        "      --post-data <data>        Send data via POST\n"
        "      --header <header>         Insert header (e.g. --header \"Accept: text/plain\")\n"
        "      --user-agent <name>       Identify as <name> to the server\n"
        "      --user <user>             HTTP server username\n"
        "      --password <pass>         HTTP server password\n"
        "      --max-redirect <n>        Maximum redirections (0 = none)\n"
        "      --spider                  Don't download; check existence (HEAD)\n"
        "  -S, --server-response         Print server response headers\n"
        "HTTPS:\n"
        "      --no-check-certificate    Don't validate the server's certificate\n"
        "Proxy:\n"
        "      --proxy <host:port>       Use HTTP proxy\n"
        "      --proxy-user <user>       Proxy username\n"
        "      --proxy-password <pass>   Proxy password\n"
        "Timeouts:\n"
        "      --timeout <secs>          Set all timeout values to SECS\n"
        "      --dns-timeout <secs>      DNS lookup timeout\n"
        "      --connect-timeout <secs>  Connect timeout\n"
        "      --read-timeout <secs>     Read (send/receive) timeout\n"
        "Misc:\n"
        "      --help                    Display this help text and exit\n"
        "      --version                 Show version information and exit\n"
    );
}

static void WgetOptions_PrintVersion(const WgetOptions* this) {
    (void)this;
    printf("wget %s\n", WGET_VERSION);
    printf("Protocols: http, https\nFeatures: TLS1.2, TLS1.3, WinHTTP-Native\n");
}

static bool WgetOptions_Parse(WgetOptions* this, int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        char* arg = argv[i];

        if (strcmp(arg, "--help") == 0) {
            this->PrintHelp(this);
            exit(EXIT_SUCCESS);
        } else if (strcmp(arg, "--version") == 0) {
            this->PrintVersion(this);
            exit(EXIT_SUCCESS);
        } else if (strcmp(arg, "-O") == 0 || strcmp(arg, "--output-document") == 0 ||
                   strcmp(arg, "-o") == 0 || strcmp(arg, "--output") == 0) {
            if (++i < argc) this->output_file = _strdup(argv[i]);
        } else if (strcmp(arg, "--content-disposition") == 0) {
            this->remote_name = true;
        } else if (strcmp(arg, "--method") == 0 || strcmp(arg, "-X") == 0 ||
                   strcmp(arg, "--request") == 0) {
            if (++i < argc) {
                free(this->method);
                this->method = _strdup(argv[i]);
            }
        } else if (strcmp(arg, "--post-data") == 0 || strcmp(arg, "-d") == 0 ||
                   strcmp(arg, "--data") == 0) {
            if (++i < argc) {
                this->post_data = _strdup(argv[i]);
                if (strcmp(this->method, "GET") == 0) {
                    free(this->method);
                    this->method = _strdup("POST");
                }
            }
        } else if (strcmp(arg, "--header") == 0 || strcmp(arg, "-H") == 0) {
            if (++i < argc) this->AddHeader(this, argv[i]);
        } else if (strcmp(arg, "--user-agent") == 0 || strcmp(arg, "-A") == 0 ||
                   strcmp(arg, "-U") == 0) {
            if (++i < argc) {
                free(this->user_agent);
                this->user_agent = _strdup(argv[i]);
            }
        } else if (strcmp(arg, "--user") == 0) {
            if (++i < argc) this->user = _strdup(argv[i]);
        } else if (strcmp(arg, "--password") == 0) {
            if (++i < argc) this->password = _strdup(argv[i]);
        } else if (strcmp(arg, "-u") == 0) {  /* curl-style user:pass */
            if (++i < argc) {
                char* colon = strchr(argv[i], ':');
                if (colon) {
                    *colon = '\0';
                    this->user = _strdup(argv[i]);
                    this->password = _strdup(colon + 1);
                    *colon = ':';
                } else {
                    this->user = _strdup(argv[i]);
                }
            }
        } else if (strcmp(arg, "--max-redirect") == 0) {
            if (++i < argc) {
                int n = 0;
                if (ParseNonNegativeInt(argv[i], &n) && n == 0)
                    this->follow_redirects = false;
            }
        } else if (strcmp(arg, "-L") == 0 || strcmp(arg, "--location") == 0) {
            this->follow_redirects = true;
        } else if (strcmp(arg, "-q") == 0 || strcmp(arg, "--quiet") == 0 ||
                   strcmp(arg, "-s") == 0 || strcmp(arg, "--silent") == 0) {
            this->quiet = true;
            this->verbose = false;
        } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            this->verbose = true;
            this->quiet = false;
        } else if (strcmp(arg, "--no-check-certificate") == 0 ||
                   strcmp(arg, "-k") == 0 || strcmp(arg, "--insecure") == 0) {
            this->insecure = true;
        } else if (strcmp(arg, "--spider") == 0 || strcmp(arg, "-I") == 0 ||
                   strcmp(arg, "--head") == 0) {
            this->head_only = true;
            free(this->method);
            this->method = _strdup("HEAD");
        } else if (strcmp(arg, "-S") == 0 || strcmp(arg, "--server-response") == 0) {
            /* print response headers; keep downloading */
            this->verbose = true;
        } else if (strcmp(arg, "-N") == 0 || strcmp(arg, "--timestamping") == 0) {
            this->timestamping = true;
        } else if (strcmp(arg, "-c") == 0 || strcmp(arg, "--continue") == 0) {
            this->continue_download = true;
        } else if (strcmp(arg, "--tries") == 0) {
            if (++i < argc) {
                int n = 0;
                if (ParseNonNegativeInt(argv[i], &n)) this->tries = n;
            }
        } else if (strcmp(arg, "--wait") == 0) {
            if (++i < argc) {
                int n = 0;
                if (ParseNonNegativeInt(argv[i], &n)) this->wait_seconds = n;
            }
        } else if (strcmp(arg, "--limit-rate") == 0) {
            if (++i < argc) this->limit_rate = ParseRate(argv[i]);
        } else if (strcmp(arg, "--no-clobber") == 0) {
            this->no_clobber = true;
        } else if (strcmp(arg, "--proxy") == 0) {
            if (++i < argc) this->proxy = _strdup(argv[i]);
        } else if (strcmp(arg, "--proxy-user") == 0) {
            if (++i < argc) this->proxy_user = _strdup(argv[i]);
        } else if (strcmp(arg, "--proxy-password") == 0) {
            if (++i < argc) this->proxy_password = _strdup(argv[i]);
        } else if (strcmp(arg, "--timeout") == 0) {
            if (++i < argc) {
                int secs = 0;
                if (ParseNonNegativeInt(argv[i], &secs)) this->timeout_ms = secs * 1000;
            }
        } else if (strcmp(arg, "--dns-timeout") == 0) {
            if (++i < argc) {
                int secs = 0;
                if (ParseNonNegativeInt(argv[i], &secs)) this->dns_timeout_ms = secs * 1000;
            }
        } else if (strcmp(arg, "--connect-timeout") == 0) {
            if (++i < argc) {
                int secs = 0;
                if (ParseNonNegativeInt(argv[i], &secs)) this->connect_timeout_ms = secs * 1000;
            }
        } else if (strcmp(arg, "--read-timeout") == 0) {
            if (++i < argc) {
                int secs = 0;
                if (ParseNonNegativeInt(argv[i], &secs)) this->read_timeout_ms = secs * 1000;
            }
        } else if (arg[0] == '-') {
            fprintf(stderr, "wget: unknown option: %s\nTry 'wget --help' for details.\n", arg);
            return false;
        } else {
            if (!this->url) {
                this->url = _strdup(arg);
            }
        }
    }

    if (!this->url) {
        fprintf(stderr, "wget: missing URL operand\nTry 'wget --help' for details.\n");
        return false;
    }

    return true;
}

WgetOptions* New_WgetOptions(void) {
    WgetOptions* opts = (WgetOptions*)malloc(sizeof(WgetOptions));
    opts->url = NULL;
    opts->method = _strdup("GET");
    opts->output_file = NULL;
    opts->remote_name = false;
    opts->post_data = NULL;
    opts->user_agent = _strdup(WGET_DEFAULT_USER_AGENT);
    opts->user = NULL;
    opts->password = NULL;
    opts->follow_redirects = true;   /* wget follows redirects by default */
    opts->quiet = false;
    opts->verbose = false;
    opts->insecure = false;
    opts->head_only = false;
    opts->timestamping = false;
    opts->continue_download = false;
    opts->tries = 1;
    opts->wait_seconds = 0;
    opts->limit_rate = 0;
    opts->no_clobber = false;
    opts->proxy = NULL;
    opts->proxy_user = NULL;
    opts->proxy_password = NULL;
    opts->timeout_ms = 0;
    opts->dns_timeout_ms = 0;
    opts->connect_timeout_ms = 0;
    opts->read_timeout_ms = 0;

    opts->header_count = 0;
    opts->header_capacity = 8;
    opts->headers = (char**)malloc(sizeof(char*) * opts->header_capacity);

    opts->Parse        = WgetOptions_Parse;
    opts->AddHeader    = WgetOptions_AddHeader;
    opts->PrintHelp    = WgetOptions_PrintHelp;
    opts->PrintVersion = WgetOptions_PrintVersion;
    return opts;
}

void Delete_WgetOptions(WgetOptions* this) {
    if (!this) return;
    free(this->url);
    free(this->method);
    free(this->output_file);
    free(this->post_data);
    free(this->user_agent);
    free(this->user);
    free(this->password);
    free(this->proxy);
    free(this->proxy_user);
    free(this->proxy_password);
    for (int i = 0; i < this->header_count; i++) free(this->headers[i]);
    free(this->headers);
    free(this);
}

/* ============================================================================
 * CLASS: WgetUrl
 * Parses and manages URL components using WinHTTP CrackUrl.
 * ============================================================================ */
typedef struct WgetUrl WgetUrl;

struct WgetUrl {
    /* Fields */
    bool   is_https;
    WCHAR* host;
    INTERNET_PORT port;
    WCHAR* path;

    /* Methods */
    bool (*Parse)(WgetUrl* this, const char* urlStr);
    char* (*ExtractFilename)(const WgetUrl* this);
};

static bool WgetUrl_Parse(WgetUrl* this, const char* urlStr) {
    WCHAR* wUrl = Utf8ToUtf16(urlStr);
    if (!wUrl) return false;

    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwHostNameLength  = (DWORD)-1;
    urlComp.dwUrlPathLength   = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(wUrl, (DWORD)wcslen(wUrl), 0, &urlComp)) {
        free(wUrl);
        return false;
    }

    this->is_https = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    this->port = urlComp.nPort;

    this->host = (WCHAR*)malloc(sizeof(WCHAR) * (urlComp.dwHostNameLength + 1));
    memcpy(this->host, urlComp.lpszHostName,
           urlComp.dwHostNameLength * sizeof(WCHAR));
    this->host[urlComp.dwHostNameLength] = L'\0';

    size_t fullPathLen = urlComp.dwUrlPathLength + urlComp.dwExtraInfoLength;
    this->path = (WCHAR*)malloc(sizeof(WCHAR) * (fullPathLen + 1));
    memcpy(this->path, urlComp.lpszUrlPath,
           urlComp.dwUrlPathLength * sizeof(WCHAR));
    if (urlComp.dwExtraInfoLength > 0) {
        memcpy(this->path + urlComp.dwUrlPathLength, urlComp.lpszExtraInfo,
               urlComp.dwExtraInfoLength * sizeof(WCHAR));
    }
    this->path[fullPathLen] = L'\0';

    free(wUrl);
    return true;
}

static char* WgetUrl_ExtractFilename(const WgetUrl* this) {
    char* fullPath = Utf16ToUtf8(this->path);
    if (!fullPath) return _strdup("index.html");

    char* lastSlash = strrchr(fullPath, '/');
    char* filename = NULL;
    if (lastSlash && *(lastSlash + 1) != '\0') {
        char* query = strchr(lastSlash + 1, '?');
        if (query) *query = '\0';
        filename = _strdup(lastSlash + 1);
    } else {
        filename = _strdup("index.html");
    }
    free(fullPath);
    return filename;
}

WgetUrl* New_WgetUrl(void) {
    WgetUrl* url = (WgetUrl*)malloc(sizeof(WgetUrl));
    url->is_https = false;
    url->host = NULL;
    url->port = 80;
    url->path = NULL;

    url->Parse           = WgetUrl_Parse;
    url->ExtractFilename = WgetUrl_ExtractFilename;
    return url;
}

void Delete_WgetUrl(WgetUrl* this) {
    if (!this) return;
    free(this->host);
    free(this->path);
    free(this);
}

/* ============================================================================
 * CLASS: WgetHttpClient
 * Coordinates the WinHTTP session, handles requests, and streams data.
 * ============================================================================ */
typedef struct WgetHttpClient WgetHttpClient;

struct WgetHttpClient {
    /* Fields */
    const WgetOptions* options;
    HINTERNET hSession;
    HINTERNET hConnect;
    HINTERNET hRequest;

    /* Methods */
    bool (*Execute)(WgetHttpClient* this);
};

/* Query a single response header as UTF-8; caller frees. */
static char* QueryHeaderUtf8(HINTERNET hRequest, DWORD infoLevel) {
    DWORD size = 0;
    WinHttpQueryHeaders(hRequest, infoLevel, WINHTTP_HEADER_NAME_BY_INDEX,
                        NULL, &size, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0) return NULL;
    WCHAR* buf = (WCHAR*)malloc(size);
    if (!buf) return NULL;
    char* result = NULL;
    if (WinHttpQueryHeaders(hRequest, infoLevel, WINHTTP_HEADER_NAME_BY_INDEX,
                            buf, &size, WINHTTP_NO_HEADER_INDEX)) {
        result = Utf16ToUtf8(buf);
    }
    free(buf);
    return result;
}

/* Extract filename from a Content-Disposition header value. */
static char* FilenameFromContentDisposition(const char* header) {
    if (!header) return NULL;
    const char* p = strstr(header, "filename=");
    if (!p) return NULL;
    p += 9;
    if (*p == '"') {
        p++;
        const char* end = strchr(p, '"');
        if (!end) return NULL;
        size_t len = (size_t)(end - p);
        char* name = (char*)malloc(len + 1);
        memcpy(name, p, len);
        name[len] = '\0';
        return name;
    }
    const char* end = strchr(p, ';');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t')) len--;
    char* name = (char*)malloc(len + 1);
    memcpy(name, p, len);
    name[len] = '\0';
    return name;
}

static void PrintProgress(long long downloaded, long long total) {
    if (total > 0) {
        int pct = (int)((downloaded * 100) / total);
        int bars = pct / 5;  /* 20-char bar */
        fprintf(stderr, "\r%lld/%lld bytes [", downloaded, total);
        for (int i = 0; i < 20; i++) fputc(i < bars ? '=' : ' ', stderr);
        fprintf(stderr, "] %d%%", pct);
    } else {
        fprintf(stderr, "\r%lld bytes", downloaded);
    }
}

/* Throttled progress: redraw at most ~10x/sec, always draw the final state. */
static void PrintProgressThrottled(long long downloaded, long long total,
                                   DWORD* lastTick, bool force) {
    DWORD now = GetTickCount();
    if (!force && (now - *lastTick) < 100) return;
    *lastTick = now;
    PrintProgress(downloaded, total);
}

static bool WgetHttpClient_Execute(WgetHttpClient* this) {
    const WgetOptions* opt = this->options;

    /* 1. Parse URL */
    WgetUrl* url = New_WgetUrl();
    if (!url->Parse(url, opt->url)) {
        if (!opt->quiet) fprintf(stderr, "wget: Could not parse URL: %s\n", opt->url);
        Delete_WgetUrl(url);
        return false;
    }

    /* 2. Open WinHTTP Session (with optional proxy) */
    WCHAR* wUserAgent = Utf8ToUtf16(opt->user_agent);
    WCHAR* wProxy = NULL;
    DWORD accessType = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;

    /* --proxy flag wins; otherwise honor http_proxy / https_proxy env vars */
    const char* proxyStr = opt->proxy;
    char* envProxy = NULL;
    if (!proxyStr) {
        if (_dupenv_s(&envProxy, NULL, url->is_https ? "https_proxy" : "http_proxy") == 0) {
            proxyStr = envProxy;
        }
    }
    if (proxyStr && *proxyStr) {
        wProxy = Utf8ToUtf16(proxyStr);
        accessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
    }

    this->hSession = WinHttpOpen(wUserAgent,
                                 accessType,
                                 wProxy ? wProxy : WINHTTP_NO_PROXY_NAME,
                                 WINHTTP_NO_PROXY_BYPASS, 0);
    free(wUserAgent);
    free(wProxy);
    free(envProxy);

    if (!this->hSession) {
        if (!opt->quiet) fprintf(stderr, "wget: Failed to initialize WinHTTP session\n");
        Delete_WgetUrl(url);
        return false;
    }

    /* Apply timeouts. --timeout sets all; per-stage flags override. */
    int dnsMs     = opt->dns_timeout_ms     ? opt->dns_timeout_ms     : opt->timeout_ms;
    int connectMs = opt->connect_timeout_ms ? opt->connect_timeout_ms : opt->timeout_ms;
    int readMs    = opt->read_timeout_ms    ? opt->read_timeout_ms    : opt->timeout_ms;
    if (dnsMs || connectMs || readMs) {
        WinHttpSetTimeouts(this->hSession,
                           dnsMs,      /* resolve */
                           connectMs,  /* connect */
                           readMs,     /* send */
                           readMs);    /* receive */
    }

    /* 3. Connect to Host */
    if (opt->verbose) {
        char* hostUtf8 = Utf16ToUtf8(url->host);
        fprintf(stderr, "* Connecting to %s (%d)...\n", hostUtf8 ? hostUtf8 : "?", url->port);
        free(hostUtf8);
    }

    this->hConnect = WinHttpConnect(this->hSession, url->host, url->port, 0);
    if (!this->hConnect) {
        if (!opt->quiet) fprintf(stderr, "wget: Connection to host failed\n");
        Delete_WgetUrl(url);
        return false;
    }

    /* 4. Open Request Handle */
    WCHAR* wMethod = Utf8ToUtf16(opt->method);
    DWORD dwFlags = url->is_https ? WINHTTP_FLAG_SECURE : 0;

    this->hRequest = WinHttpOpenRequest(this->hConnect,
                                        wMethod,
                                        url->path,
                                        NULL,
                                        WINHTTP_NO_REFERER,
                                        WINHTTP_DEFAULT_ACCEPT_TYPES,
                                        dwFlags);
    free(wMethod);

    if (!this->hRequest) {
        if (!opt->quiet) fprintf(stderr, "wget: Failed to create HTTP request\n");
        Delete_WgetUrl(url);
        return false;
    }

    /* Insecure TLS (--no-check-certificate) */
    if (opt->insecure && url->is_https) {
        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                         SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                         SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                         SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(this->hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
    }

    /* Redirect policy (wget follows by default; --max-redirect 0 disables) */
    DWORD redirectPolicy = opt->follow_redirects ?
                           WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS :
                           WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(this->hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    /* Server auth (--user / --password) */
    if (opt->user) {
        WCHAR* wUser = Utf8ToUtf16(opt->user);
        WCHAR* wPass = Utf8ToUtf16(opt->password ? opt->password : "");
        WinHttpSetCredentials(this->hRequest, WINHTTP_AUTH_TARGET_SERVER,
                              WINHTTP_AUTH_SCHEME_BASIC, wUser, wPass, NULL);
        free(wUser);
        free(wPass);
    }

    /* Proxy auth (--proxy-user / --proxy-password) */
    if (opt->proxy_user) {
        WCHAR* wUser = Utf8ToUtf16(opt->proxy_user);
        WCHAR* wPass = Utf8ToUtf16(opt->proxy_password ? opt->proxy_password : "");
        WinHttpSetCredentials(this->hRequest, WINHTTP_AUTH_TARGET_PROXY,
                              WINHTTP_AUTH_SCHEME_BASIC, wUser, wPass, NULL);
        free(wUser);
        free(wPass);
    }

    /* Determine the local target filename (explicit -O or derived from URL). */
    char* derivedTarget = NULL;
    const char* targetFile = opt->output_file;
    if (!targetFile) {
        derivedTarget = url->ExtractFilename(url);
        targetFile = derivedTarget;
    }

    /* Resume support (-c): send Range header for existing partial file */
    long long resumeOffset = 0;
    if (opt->continue_download && targetFile) {
        FILE* f = NULL;
        if (fopen_s(&f, targetFile, "rb") == 0 && f) {
            _fseeki64(f, 0, SEEK_END);
            resumeOffset = _ftelli64(f);
            fclose(f);
            if (resumeOffset > 0) {
                WCHAR rangeHdr[64];
                _snwprintf_s(rangeHdr, 64, _TRUNCATE, L"Range: bytes=%lld-", resumeOffset);
                WinHttpAddRequestHeaders(this->hRequest, rangeHdr, (DWORD)-1,
                                         WINHTTP_ADDREQ_FLAG_ADD);
            }
        }
    }

    /* Timestamping (-N): send If-Modified-Since from the local file's mtime */
    if (opt->timestamping && targetFile) {
        FILETIME lastWrite;
        HANDLE hFile = CreateFileA(targetFile, GENERIC_READ, FILE_SHARE_READ, NULL,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            if (GetFileTime(hFile, NULL, NULL, &lastWrite)) {
                SYSTEMTIME st;
                if (FileTimeToSystemTime(&lastWrite, &st)) {
                    /* HTTP-date: "Wed, 21 Oct 2015 07:28:00 GMT" */
                    static const char* const days[] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
                    static const char* const months[] = { "Jan","Feb","Mar","Apr","May","Jun",
                                                          "Jul","Aug","Sep","Oct","Nov","Dec" };
                    char ims[64];
                    snprintf(ims, sizeof(ims), "If-Modified-Since: %s, %02d %s %04d %02d:%02d:%02d GMT",
                             days[st.wDayOfWeek], st.wDay, months[st.wMonth - 1], st.wYear,
                             st.wHour, st.wMinute, st.wSecond);
                    WCHAR* wIms = Utf8ToUtf16(ims);
                    WinHttpAddRequestHeaders(this->hRequest, wIms, (DWORD)-1,
                                             WINHTTP_ADDREQ_FLAG_ADD);
                    free(wIms);
                }
            }
            CloseHandle(hFile);
        }
    }

    /* Add Custom Headers (--header) */
    for (int i = 0; i < opt->header_count; i++) {
        WCHAR* wHeader = Utf8ToUtf16(opt->headers[i]);
        WinHttpAddRequestHeaders(this->hRequest, wHeader, (DWORD)-1,
                                 WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        if (opt->verbose) {
            fprintf(stderr, "* > %s\n", opt->headers[i]);
        }
        free(wHeader);
    }

    /* 5. Send Request (with retry loop per --tries/--wait) */
    DWORD dataLen = opt->post_data ? (DWORD)strlen(opt->post_data) : 0;
    if (opt->verbose) {
        char* pathUtf8 = Utf16ToUtf8(url->path);
        fprintf(stderr, "* > %s %s HTTP/1.1\n", opt->method, pathUtf8 ? pathUtf8 : "/");
        free(pathUtf8);
    }

    BOOL bResults = FALSE;
    int attempt = 0;
    int maxTries = (opt->tries == 0) ? 20 : opt->tries;  /* 0 = "infinite", cap at 20 */
    for (;;) {
        attempt++;
        bResults = WinHttpSendRequest(this->hRequest,
                                      WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                      (LPVOID)opt->post_data, dataLen,
                                      dataLen, 0);
        if (bResults) {
            bResults = WinHttpReceiveResponse(this->hRequest, NULL);
        }
        if (bResults) break;
        if (attempt >= maxTries) break;
        if (!opt->quiet) {
            fprintf(stderr, "wget: attempt %d failed (Error: %lu), retrying...\n",
                    attempt, GetLastError());
        }
        if (opt->wait_seconds > 0) Sleep((DWORD)opt->wait_seconds * 1000);
    }

    if (!bResults) {
        if (!opt->quiet) fprintf(stderr, "wget: Failed to send/receive request (Error: %lu)\n", GetLastError());
        Delete_WgetUrl(url);
        return false;
    }

    /* 6. Read Response Status */
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(this->hRequest,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

    /* Print response headers for verbose / spider / -S */
    if (opt->verbose || opt->head_only) {
        char* rawHeaders = QueryHeaderUtf8(this->hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF);
        if (rawHeaders) {
            if (opt->head_only) {
                printf("%s", rawHeaders);
            } else {
                fprintf(stderr, "< %s\n", rawHeaders);
            }
            free(rawHeaders);
        }
    }

    /* Timestamping: 304 Not Modified means local file is current — skip. */
    if (opt->timestamping && statusCode == 304) {
        if (!opt->quiet) {
            fprintf(stderr, "wget: %s not modified on server, skipping\n", targetFile);
        }
        free(derivedTarget);
        Delete_WgetUrl(url);
        return true;
    }

    /* HTTP status-code handling: fail on 4xx/5xx (wget behavior) */
    if (statusCode >= 400) {
        if (!opt->quiet) {
            fprintf(stderr, "wget: server returned error: HTTP %lu\n", statusCode);
        }
        free(derivedTarget);
        Delete_WgetUrl(url);
        return false;
    }

    /* Resume integrity: if we sent a Range but got 200 (not 206), the server
     * ignored it — restart the download from scratch to avoid corruption. */
    if (resumeOffset > 0 && statusCode == 200) {
        if (!opt->quiet) {
            fprintf(stderr, "wget: server ignored resume request, restarting download\n");
        }
        resumeOffset = 0;
    }

    /* If --spider / HEAD was specified, stop here */
    if (opt->head_only) {
        free(derivedTarget);
        Delete_WgetUrl(url);
        return true;
    }

    /* Content-Length for progress tracking */
    long long contentLength = -1;
    {
        DWORD clSize = 0;
        WinHttpQueryHeaders(this->hRequest, WINHTTP_QUERY_CONTENT_LENGTH,
                            WINHTTP_HEADER_NAME_BY_INDEX, NULL, &clSize, WINHTTP_NO_HEADER_INDEX);
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && clSize > 0) {
            WCHAR* clBuf = (WCHAR*)malloc(clSize);
            if (clBuf && WinHttpQueryHeaders(this->hRequest, WINHTTP_QUERY_CONTENT_LENGTH,
                                             WINHTTP_HEADER_NAME_BY_INDEX, clBuf, &clSize,
                                             WINHTTP_NO_HEADER_INDEX)) {
                contentLength = _wtoi64(clBuf);
            }
            free(clBuf);
        }
    }

    /* 7. Setup Output Stream Destination */
    FILE* outStream = stdout;
    bool mustCloseFile = false;
    const char* openMode = (opt->continue_download && resumeOffset > 0) ? "ab" : "wb";

    /* --no-clobber: skip if the target file already exists */
    if (opt->no_clobber && targetFile) {
        FILE* existing = NULL;
        if (fopen_s(&existing, targetFile, "rb") == 0 && existing) {
            fclose(existing);
            if (!opt->quiet) {
                fprintf(stderr, "wget: %s already exists, skipping (--no-clobber)\n", targetFile);
            }
            free(derivedTarget);
            Delete_WgetUrl(url);
            return true;
        }
    }

    if (opt->output_file) {
        if (fopen_s(&outStream, opt->output_file, openMode) != 0) {
            outStream = NULL;
        }
        mustCloseFile = true;
    } else if (opt->remote_name) {
        /* --content-disposition: honor server filename, else derive from URL */
        char* filename = NULL;
        char* cd = QueryHeaderUtf8(this->hRequest, WINHTTP_QUERY_CONTENT_DISPOSITION);
        if (cd) {
            filename = FilenameFromContentDisposition(cd);
            free(cd);
        }
        if (!filename) filename = url->ExtractFilename(url);

        if (fopen_s(&outStream, filename, "wb") != 0) {
            outStream = NULL;
        }
        if (opt->verbose) {
            fprintf(stderr, "* Saving to: %s\n", filename);
        }
        free(filename);
        mustCloseFile = true;
    } else {
        /* Default wget behavior: save to file named from URL */
        char* filename = url->ExtractFilename(url);
        if (fopen_s(&outStream, filename, openMode) != 0) {
            outStream = NULL;
        }
        if (opt->verbose) {
            fprintf(stderr, "* Saving to: %s\n", filename);
        }
        free(filename);
        mustCloseFile = true;
    }

    if (!outStream) {
        if (!opt->quiet) fprintf(stderr, "wget: Cannot open target file for writing\n");
        free(derivedTarget);
        Delete_WgetUrl(url);
        return false;
    }

    /* 8. Stream Response Data with throttled progress and optional rate limit */
    DWORD bytesRead = 0;
    DWORD bytesAvailable = 0;
    char buffer[BUFFER_SIZE];
    long long totalDownloaded = resumeOffset;
    bool showProgress = !opt->quiet && mustCloseFile;  /* progress to stderr when saving */
    DWORD lastProgressTick = 0;
    DWORD transferStart = GetTickCount();

    do {
        bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(this->hRequest, &bytesAvailable)) break;
        if (bytesAvailable == 0) break;

        DWORD toRead = (bytesAvailable < BUFFER_SIZE) ? bytesAvailable : BUFFER_SIZE;
        if (WinHttpReadData(this->hRequest, (LPVOID)buffer, toRead, &bytesRead)) {
            if (bytesRead > 0) {
                fwrite(buffer, 1, bytesRead, outStream);
                totalDownloaded += bytesRead;
                if (showProgress) {
                    PrintProgressThrottled(totalDownloaded, contentLength, &lastProgressTick, false);
                }

                /* --limit-rate: throttle to the configured bytes/sec */
                if (opt->limit_rate > 0) {
                    DWORD elapsed = GetTickCount() - transferStart;
                    long long expectedMs = (totalDownloaded * 1000LL) / opt->limit_rate;
                    if (expectedMs > (long long)elapsed) {
                        Sleep((DWORD)(expectedMs - elapsed));
                    }
                }
            }
        }
    } while (bytesAvailable > 0);

    if (showProgress) {
        PrintProgressThrottled(totalDownloaded, contentLength, &lastProgressTick, true);
        fprintf(stderr, "\nwget: saved %lld bytes\n", totalDownloaded);
    }

    if (mustCloseFile) {
        fclose(outStream);
    } else {
        fflush(stdout);
    }

    free(derivedTarget);
    Delete_WgetUrl(url);
    return true;
}

WgetHttpClient* New_WgetHttpClient(const WgetOptions* options) {
    WgetHttpClient* client = (WgetHttpClient*)malloc(sizeof(WgetHttpClient));
    client->options = options;
    client->hSession = NULL;
    client->hConnect = NULL;
    client->hRequest = NULL;

    client->Execute = WgetHttpClient_Execute;
    return client;
}

void Delete_WgetHttpClient(WgetHttpClient* this) {
    if (!this) return;
    if (this->hRequest) WinHttpCloseHandle(this->hRequest);
    if (this->hConnect) WinHttpCloseHandle(this->hConnect);
    if (this->hSession) WinHttpCloseHandle(this->hSession);
    free(this);
}

/* ============================================================================
 * MAIN ENTRY POINT
 * ============================================================================ */
int main(int argc, char** argv) {
    WgetOptions* options = New_WgetOptions();
    if (!options->Parse(options, argc, argv)) {
        Delete_WgetOptions(options);
        return EXIT_FAILURE;
    }

    WgetHttpClient* client = New_WgetHttpClient(options);
    bool success = client->Execute(client);

    Delete_WgetHttpClient(client);
    Delete_WgetOptions(options);

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
