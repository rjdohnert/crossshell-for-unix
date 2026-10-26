#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <io.h>

#pragma comment(lib, "advapi32.lib")

/* ========================================================================= */
/*                          1. NT NATIVE DEFINITIONS                         */
/* ========================================================================= */

#define STATUS_SUCCESS               ((NTSTATUS)0x00000000L)
#define STATUS_NO_MORE_ENTRIES       ((NTSTATUS)0x8000001AL)
#define STATUS_BUFFER_TOO_SMALL      ((NTSTATUS)0xC0000023L)
#define DIRECTORY_QUERY              (0x0001)

typedef LONG NTSTATUS;

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
    ULONG           Length;
    HANDLE          RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG           Attributes;
    PVOID           SecurityDescriptor;
    PVOID           SecurityQualityOfService;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

#define InitializeObjectAttributes(p, n, a, r, s) { \
    (p)->Length = sizeof(OBJECT_ATTRIBUTES);          \
    (p)->RootDirectory = r;                          \
    (p)->Attributes = a;                             \
    (p)->ObjectName = n;                             \
    (p)->SecurityDescriptor = s;                     \
    (p)->SecurityQualityOfService = NULL;            \
}

typedef struct _OBJECT_DIRECTORY_INFORMATION {
    UNICODE_STRING Name;
    UNICODE_STRING TypeName;
} OBJECT_DIRECTORY_INFORMATION, *POBJECT_DIRECTORY_INFORMATION;

typedef NTSTATUS (NTAPI *pfnNtOpenDirectoryObject)(
    PHANDLE DirectoryHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes
);

typedef NTSTATUS (NTAPI *pfnNtQueryDirectoryObject)(
    HANDLE DirectoryHandle,
    PVOID Buffer,
    ULONG Length,
    BOOLEAN ReturnSingleEntry,
    BOOLEAN RestartScan,
    PULONG Context,
    PULONG ReturnLength
);

/* ========================================================================= */
/*                          2. OBJECT-ORIENTED MODEL                         */
/* ========================================================================= */

typedef struct _ObjectItem {
    wchar_t* name;
    wchar_t* type;
    wchar_t* path;
} ObjectItem;

typedef struct _ObjectCollection {
    ObjectItem* items;
    size_t count;
    size_t capacity;

    void (*add)(struct _ObjectCollection* self, const wchar_t* name, const wchar_t* type, const wchar_t* path);
    void (*free)(struct _ObjectCollection* self);
} ObjectCollection;

static void collection_add(ObjectCollection* self, const wchar_t* name, const wchar_t* type, const wchar_t* path) {
    if (self->count >= self->capacity) {
        self->capacity = (self->capacity == 0) ? 64 : self->capacity * 2;
        self->items = (ObjectItem*)realloc(self->items, self->capacity * sizeof(ObjectItem));
    }
    self->items[self->count].name = _wcsdup(name ? name : L"");
    self->items[self->count].type = _wcsdup(type ? type : L"");
    self->items[self->count].path = _wcsdup(path ? path : L"");
    self->count++;
}

static void collection_free(ObjectCollection* self) {
    if (!self->items) return;
    for (size_t i = 0; i < self->count; i++) {
        free(self->items[i].name);
        free(self->items[i].type);
        free(self->items[i].path);
    }
    free(self->items);
    self->items = NULL;
    self->count = 0;
    self->capacity = 0;
}

void ObjectCollection_Init(ObjectCollection* col) {
    col->items = NULL;
    col->count = 0;
    col->capacity = 0;
    col->add = collection_add;
    col->free = collection_free;
}

/* ========================================================================= */
/*                          3. FORMATTER INTERFACE (OOP)                     */
/* ========================================================================= */

typedef struct _IFormatter IFormatter;
typedef struct _IFormatterVTable {
    void (*format)(IFormatter* self, const ObjectCollection* col, FILE* out);
} IFormatterVTable;

struct _IFormatter {
    const IFormatterVTable* vptr;
};

/* --- Table Formatter --- */
static void table_format(IFormatter* self, const ObjectCollection* col, FILE* out) {
    (void)self;
    size_t max_name = 4, max_type = 4, max_path = 4;
    for (size_t i = 0; i < col->count; i++) {
        size_t ln = wcslen(col->items[i].name);
        size_t lt = wcslen(col->items[i].type);
        size_t lp = wcslen(col->items[i].path);
        if (ln > max_name) max_name = ln;
        if (lt > max_type) max_type = lt;
        if (lp > max_path) max_path = lp;
    }

    if (max_name > 45) max_name = 45;
    if (max_path > 60) max_path = 60;

    fwprintf(out, L"%-*ls  %-*ls  %-*ls\n", (int)max_type, L"TYPE", (int)max_name, L"NAME", (int)max_path, L"NAMESPACE PATH");
    
    for (size_t i = 0; i < max_name + max_type + max_path + 4; i++) fputwc(L'-', out);
    fputwc(L'\n', out);

    for (size_t i = 0; i < col->count; i++) {
        fwprintf(out, L"%-*ls  %-*ls  %-*ls\n",
            (int)max_type, col->items[i].type,
            (int)max_name, col->items[i].name,
            (int)max_path, col->items[i].path);
    }
}
static const IFormatterVTable TableVTable = { table_format };

/* --- JSON Formatter --- */
static void json_print_escaped(FILE* out, const wchar_t* str) {
    fputwc(L'"', out);
    while (*str) {
        switch (*str) {
            case L'\\': fputws(L"\\\\", out); break;
            case L'"':  fputws(L"\\\"", out); break;
            case L'\b': fputws(L"\\b", out);  break;
            case L'\f': fputws(L"\\f", out);  break;
            case L'\n': fputws(L"\\n", out);  break;
            case L'\r': fputws(L"\\r", out);  break;
            case L'\t': fputws(L"\\t", out);  break;
            default:    fputwc(*str, out);    break;
        }
        str++;
    }
    fputwc(L'"', out);
}

static void json_format(IFormatter* self, const ObjectCollection* col, FILE* out) {
    (void)self;
    fputws(L"[\n", out);
    for (size_t i = 0; i < col->count; i++) {
        fputws(L"  {\n", out);
        fputws(L"    \"type\": ", out);
        json_print_escaped(out, col->items[i].type);
        fputws(L",\n    \"name\": ", out);
        json_print_escaped(out, col->items[i].name);
        fputws(L",\n    \"path\": ", out);
        json_print_escaped(out, col->items[i].path);
        fputws(L"\n  }", out);
        if (i + 1 < col->count) fputwc(L',', out);
        fputwc(L'\n', out);
    }
    fputws(L"]\n", out);
}
static const IFormatterVTable JsonVTable = { json_format };

/* --- CSV Formatter --- */
static void csv_format(IFormatter* self, const ObjectCollection* col, FILE* out) {
    (void)self;
    fputws(L"Type,Name,Path\n", out);
    for (size_t i = 0; i < col->count; i++) {
        fwprintf(out, L"\"%ls\",\"%ls\",\"%ls\"\n",
            col->items[i].type, col->items[i].name, col->items[i].path);
    }
}
static const IFormatterVTable CsvVTable = { csv_format };

/* --- Raw / Piped Formatter (Unix line-based for awk, grep, etc.) --- */
static void raw_format(IFormatter* self, const ObjectCollection* col, FILE* out) {
    (void)self;
    for (size_t i = 0; i < col->count; i++) {
        fwprintf(out, L"%ls\t%ls\t%ls\n",
            col->items[i].type, col->items[i].name, col->items[i].path);
    }
}
static const IFormatterVTable RawVTable = { raw_format };

/* ========================================================================= */
/*                          4. NT BROWSER ENGINE                             */
/* ========================================================================= */

typedef struct _NtBrowser {
    pfnNtOpenDirectoryObject  NtOpenDirectoryObject;
    pfnNtQueryDirectoryObject NtQueryDirectoryObject;
    bool initialized;

    bool (*init)(struct _NtBrowser* self);
    void (*scan_nt_directory)(struct _NtBrowser* self, const wchar_t* dirPath, const wchar_t* typeFilter, ObjectCollection* col);
    void (*scan_pipes)(struct _NtBrowser* self, ObjectCollection* col);
    void (*close)(struct _NtBrowser* self);
} NtBrowser;

static bool browser_init(NtBrowser* self) {
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) return false;

    self->NtOpenDirectoryObject = (pfnNtOpenDirectoryObject)GetProcAddress(hNtdll, "NtOpenDirectoryObject");
    self->NtQueryDirectoryObject = (pfnNtQueryDirectoryObject)GetProcAddress(hNtdll, "NtQueryDirectoryObject");

    self->initialized = (self->NtOpenDirectoryObject && self->NtQueryDirectoryObject);
    return self->initialized;
}

static void browser_scan_nt_dir(NtBrowser* self, const wchar_t* dirPath, const wchar_t* typeFilter, ObjectCollection* col) {
    if (!self->initialized) return;

    HANDLE hDirectory = NULL;
    UNICODE_STRING uPath;
    uPath.Buffer = (PWSTR)dirPath;
    uPath.Length = (USHORT)(wcslen(dirPath) * sizeof(wchar_t));
    uPath.MaximumLength = uPath.Length + sizeof(wchar_t);

    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, &uPath, 0, NULL, NULL);

    NTSTATUS status = self->NtOpenDirectoryObject(&hDirectory, DIRECTORY_QUERY, &oa);
    if (status != STATUS_SUCCESS) return;

    BYTE buffer[4096];
    ULONG context = 0;
    ULONG returnLength = 0;

    while (true) {
        status = self->NtQueryDirectoryObject(
            hDirectory,
            buffer,
            sizeof(buffer),
            FALSE,
            (context == 0),
            &context,
            &returnLength
        );

        if (status != STATUS_SUCCESS && status != STATUS_NO_MORE_ENTRIES) break;

        POBJECT_DIRECTORY_INFORMATION info = (POBJECT_DIRECTORY_INFORMATION)buffer;
        while (info->Name.Buffer != NULL) {
            wchar_t name[MAX_PATH] = {0};
            wchar_t type[MAX_PATH] = {0};

            wcsncpy_s(name, MAX_PATH, info->Name.Buffer, info->Name.Length / sizeof(wchar_t));
            wcsncpy_s(type, MAX_PATH, info->TypeName.Buffer, info->TypeName.Length / sizeof(wchar_t));

            if (!typeFilter || _wcsicmp(type, typeFilter) == 0) {
                wchar_t fullPath[MAX_PATH * 2];
                if (wcscmp(dirPath, L"\\") == 0) {
                    swprintf_s(fullPath, MAX_PATH * 2, L"\\%ls", name);
                } else {
                    swprintf_s(fullPath, MAX_PATH * 2, L"%ls\\%ls", dirPath, name);
                }
                col->add(col, name, type, fullPath);
            }

            info++;
        }

        if (status == STATUS_NO_MORE_ENTRIES) break;
    }

    CloseHandle(hDirectory);
}

static void browser_scan_pipes(NtBrowser* self, ObjectCollection* col) {
    (void)self;
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(L"\\\\.\\pipe\\*", &ffd);

    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        wchar_t fullPath[MAX_PATH * 2];
        swprintf_s(fullPath, MAX_PATH * 2, L"\\Device\\NamedPipe\\%ls", ffd.cFileName);
        col->add(col, ffd.cFileName, L"NamedPipe", fullPath);
    } while (FindNextFileW(hFind, &ffd) != 0);

    FindClose(hFind);
}

static void browser_close(NtBrowser* self) {
    self->initialized = false;
}

void NtBrowser_Init(NtBrowser* browser) {
    browser->initialized = false;
    browser->init = browser_init;
    browser->scan_nt_directory = browser_scan_nt_dir;
    browser->scan_pipes = browser_scan_pipes;
    browser->close = browser_close;
}

/* ========================================================================= */
/*                          5. CLI & HELP ENGINE                             */
/* ========================================================================= */

typedef enum {
    MODE_ALL,
    MODE_PIPES,
    MODE_MUTEXES,
    MODE_EVENTS,
    MODE_SECTIONS,
    MODE_CUSTOM_DIR
} RunMode;

typedef enum {
    FMT_AUTO,
    FMT_TABLE,
    FMT_JSON,
    FMT_CSV,
    FMT_RAW
} FormatType;

static void show_help(void) {
    wprintf(L"winobj - Windows NT Object Manager Browser\n\n");
    wprintf(L"USAGE:\n");
    wprintf(L"  winobj [OBJECT_SWITCH] [OPTIONS]\n\n");
    wprintf(L"OBJECT SWITCHES:\n");
    wprintf(L"  -pipes,    --pipes       Enumerate named pipes (\\\\.\\pipe\\*)\n");
    wprintf(L"  -mutexes,  --mutexes     Enumerate Mutants/Mutexes in \\BaseNamedObjects\n");
    wprintf(L"  -events,   --events      Enumerate Events in \\BaseNamedObjects\n");
    wprintf(L"  -sections, --sections    Enumerate Memory Sections in \\BaseNamedObjects\n");
    wprintf(L"  -dir <path>              Enumerate any NT directory (e.g. \\Device, \\RPC Control, \\)\n\n");
    wprintf(L"OPTIONS:\n");
    wprintf(L"  -f, --format <format>    Output format: table, json, csv, raw (default: auto)\n");
    wprintf(L"  -S, --session <id>       Target terminal session (e.g. \\Sessions\\1\\BaseNamedObjects)\n");
    wprintf(L"  -h, --help               Display this help page\n\n");
    wprintf(L"PIPING & COMPATIBILITY:\n");
    wprintf(L"  Output defaults to 'raw' (TSV) when standard output is redirected/piped,\n");
    wprintf(L"  and 'table' when connected directly to a terminal (TTY).\n\n");
    wprintf(L"EXAMPLES:\n");
    wprintf(L"  winobj -pipes\n");
    wprintf(L"  winobj -mutexes --format json\n");
    wprintf(L"  winobj -events | grep -i Global\n");
    wprintf(L"  winobj -dir \"\\RPC Control\" --format csv\n");
    wprintf(L"  winobj -mutexes -S 1\n");
}

/* ========================================================================= */
/*                                6. MAIN                                    */
/* ========================================================================= */

int wmain(int argc, wchar_t* argv[]) {
    RunMode mode = MODE_ALL;
    FormatType fmt = FMT_AUTO;
    wchar_t customDir[MAX_PATH] = L"\\BaseNamedObjects";
    int session_id = -1;

    for (int i = 1; i < argc; i++) {
        if (_wcsicmp(argv[i], L"-h") == 0 || _wcsicmp(argv[i], L"--help") == 0 || _wcsicmp(argv[i], L"/?") == 0) {
            show_help();
            return 0;
        } else if (_wcsicmp(argv[i], L"-pipes") == 0 || _wcsicmp(argv[i], L"--pipes") == 0) {
            mode = MODE_PIPES;
        } else if (_wcsicmp(argv[i], L"-mutexes") == 0 || _wcsicmp(argv[i], L"--mutexes") == 0) {
            mode = MODE_MUTEXES;
        } else if (_wcsicmp(argv[i], L"-events") == 0 || _wcsicmp(argv[i], L"--events") == 0) {
            mode = MODE_EVENTS;
        } else if (_wcsicmp(argv[i], L"-sections") == 0 || _wcsicmp(argv[i], L"--sections") == 0) {
            mode = MODE_SECTIONS;
        } else if (_wcsicmp(argv[i], L"-dir") == 0 && i + 1 < argc) {
            mode = MODE_CUSTOM_DIR;
            wcscpy_s(customDir, MAX_PATH, argv[++i]);
        } else if ((_wcsicmp(argv[i], L"-f") == 0 || _wcsicmp(argv[i], L"--format") == 0) && i + 1 < argc) {
            wchar_t* f = argv[++i];
            if (_wcsicmp(f, L"json") == 0)       fmt = FMT_JSON;
            else if (_wcsicmp(f, L"csv") == 0)  fmt = FMT_CSV;
            else if (_wcsicmp(f, L"table") == 0) fmt = FMT_TABLE;
            else if (_wcsicmp(f, L"raw") == 0)   fmt = FMT_RAW;
            else {
                fwprintf(stderr, L"Unknown format: %ls\n", f);
                return 1;
            }
        } else if ((_wcsicmp(argv[i], L"-S") == 0 || _wcsicmp(argv[i], L"--session") == 0) && i + 1 < argc) {
            session_id = _wtoi(argv[++i]);
        } else {
            fwprintf(stderr, L"Unknown parameter: %ls\nRun 'winobj --help' for usage.\n", argv[i]);
            return 1;
        }
    }

    // Determine default output format based on stdout redirection
    if (fmt == FMT_AUTO) {
        fmt = _isatty(_fileno(stdout)) ? FMT_TABLE : FMT_RAW;
    }

    // Prepare Object Namespace path
    wchar_t targetPath[MAX_PATH];
    if (session_id >= 0) {
        swprintf_s(targetPath, MAX_PATH, L"\\Sessions\\%d\\BaseNamedObjects", session_id);
    } else if (mode == MODE_CUSTOM_DIR) {
        wcscpy_s(targetPath, MAX_PATH, customDir);
    } else {
        wcscpy_s(targetPath, MAX_PATH, L"\\BaseNamedObjects");
    }

    // Initialize Engine and Data Collection
    NtBrowser browser;
    NtBrowser_Init(&browser);
    if (!browser.init(&browser)) {
        fwprintf(stderr, L"Failed to bind NT Object Manager APIs.\n");
        return 1;
    }

    ObjectCollection collection;
    ObjectCollection_Init(&collection);

    // Collect requested namespaces
    switch (mode) {
        case MODE_PIPES:
            browser.scan_pipes(&browser, &collection);
            break;
        case MODE_MUTEXES:
            browser.scan_nt_directory(&browser, targetPath, L"Mutant", &collection);
            break;
        case MODE_EVENTS:
            browser.scan_nt_directory(&browser, targetPath, L"Event", &collection);
            break;
        case MODE_SECTIONS:
            browser.scan_nt_directory(&browser, targetPath, L"Section", &collection);
            break;
        case MODE_CUSTOM_DIR:
        case MODE_ALL:
        default:
            browser.scan_nt_directory(&browser, targetPath, NULL, &collection);
            break;
    }

    // Select and instantiate Formatter
    IFormatter formatter;
    switch (fmt) {
        case FMT_JSON:  formatter.vptr = &JsonVTable;  break;
        case FMT_CSV:   formatter.vptr = &CsvVTable;   break;
        case FMT_RAW:   formatter.vptr = &RawVTable;   break;
        case FMT_TABLE:
        default:        formatter.vptr = &TableVTable; break;
    }

    // Output formatted stream
    formatter.vptr->format(&formatter, &collection, stdout);

    // Clean up
    collection.free(&collection);
    browser.close(&browser);

    return 0;
}