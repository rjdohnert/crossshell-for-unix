/**
 * xls - Extended ls for Windows NT
 * A fully functional, Object-Oriented Extended Directory Lister.
 *
 * Copyright (C) 2026 PC/OpenSystems LLC.
 * All Rights Reserved
 *
 * Licensed under the terms of the BSD 3-Clause License.
 * 
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

#define XLS_VERSION "2.1.0"
#define MAX_PATH_EXT 32768

/* ========================================================================= */
/* 1. CLI OPTIONS CLASS                                                      */
/* ========================================================================= */

typedef enum {
    SORT_NAME,
    SORT_EXT,
    SORT_SIZE,
    SORT_DATE
} SortCriteria;

typedef struct CliOptions {
    bool showAll;         /* -a, --all (show hidden/system) */
    bool longListing;     /* -l, --long */
    bool showClusters;    /* -c, --clusters  */
    bool recursive;       /* -R, --recursive */
    bool humanReadable;   /* -h, --human-readable */
    bool reverseSort;     /* -r, --reverse */
    bool noColor;         /* -n, --no-color */
    bool showHelp;        /* -H, --help */
    SortCriteria sortBy;  /* -s, --sort <name|ext|size|date> */
    wchar_t targetPath[MAX_PATH_EXT];

    /* Member Methods */
    bool (*Parse)(struct CliOptions* this, int argc, char** argv);
    void (*PrintHelp)(const struct CliOptions* this);
} CliOptions;

static void CliOptions_PrintHelp(const CliOptions* this) {
    (void)this;
    wprintf(
        L"NAME\n"
        L"    xls - Extended ls for Windows\n\n"
        L"SYNOPSIS\n"
        L"    xls [OPTIONS...] [PATH]\n\n"
        L"DESCRIPTION\n"
        L"    xls reproduces the extended file telemetry for Windows NT filesystem.\n"
        L"    Displays raw file sizes alongside sector/cluster allocations, disk slack,\n"
        L"    extended NTFS/DOS file attributes (RHSADCE), and volume cluster usage.\n\n"
        L"OPTIONS\n"
        L"    -a, --all\n"
        L"        Include hidden and system files (hidden by default).\n"
        L"    -l, --long\n"
        L"        Full extended listing (timestamps, allocations, attributes, names).\n"
        L"    -c, --clusters\n"
        L"        Display allocated cluster count per file.\n"
        L"    -s, --sort <name|ext|size|date>\n"
        L"        Sort entries by criteria. Default is 'name'.\n"
        L"    -r, --reverse\n"
        L"        Reverse order while sorting.\n"
        L"    -h, --human-readable\n"
        L"        (Default) Display capacities with unit suffixes (B, KB, MB, GB, TB).\n"
        L"    -V, --version\n"
        L"        Display version information and exit.\n"
        L"    -R, --recursive\n"
        L"        Recursively scan and list subdirectories.\n"
        L"    -n, --no-color\n"
        L"        Disable ANSI terminal coloring.\n"
        L"    -H, --help\n"
        L"        Display this manual and exit.\n\n"
        L"NTFS ATTRIBUTE FLAGS\n"
        L"    R = Read-Only      A = Archive         C = Compressed\n"
        L"    H = Hidden         D = Directory       E = Encrypted\n"
        L"    S = System         L = Reparse/Link\n\n"
        L"EXAMPLES\n"
        L"    xls -l -c C:\\Windows\n"
        L"    xls -l -s size -r -h\n"
        L"    xls -R -a -l\n"
    );
}

static bool CliOptions_Parse(CliOptions* this, int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        char* arg = argv[i];

        if (strcmp(arg, "-H") == 0 || strcmp(arg, "--help") == 0) {
            this->showHelp = true;
            return true;
        } else if (strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0) {
            wprintf(L"xls version %hs\n", XLS_VERSION);
            wprintf(L"Copyright (C) 2026 PC/OpenSystems LLC.\n");
            exit(EXIT_SUCCESS);
        } else if (strcmp(arg, "-a") == 0 || strcmp(arg, "--all") == 0) {
            this->showAll = true;
        } else if (strcmp(arg, "-l") == 0 || strcmp(arg, "--long") == 0) {
            this->longListing = true;
        } else if (strcmp(arg, "-c") == 0 || strcmp(arg, "--clusters") == 0) {
            this->showClusters = true;
            this->longListing = true;
        } else if (strcmp(arg, "-R") == 0 || strcmp(arg, "--recursive") == 0) {
            this->recursive = true;
        } else if (strcmp(arg, "-r") == 0 || strcmp(arg, "--reverse") == 0) {
            this->reverseSort = true;
        } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--human-readable") == 0) {
            this->humanReadable = true;
        } else if (strcmp(arg, "-n") == 0 || strcmp(arg, "--no-color") == 0) {
            this->noColor = true;
        } else if (strcmp(arg, "-s") == 0 || strcmp(arg, "--sort") == 0) {
            if (i + 1 < argc) {
                char* criteria = argv[++i];
                if (_stricmp(criteria, "name") == 0)      this->sortBy = SORT_NAME;
                else if (_stricmp(criteria, "ext") == 0)  this->sortBy = SORT_EXT;
                else if (_stricmp(criteria, "size") == 0) this->sortBy = SORT_SIZE;
                else if (_stricmp(criteria, "date") == 0) this->sortBy = SORT_DATE;
                else {
                    fprintf(stderr, "xls: Invalid sort criteria '%s'.\n", criteria);
                    return false;
                }
            } else {
                fprintf(stderr, "xls: Option '%s' requires an argument.\n", arg);
                return false;
            }
        } else if (arg[0] == '-') {
            fprintf(stderr, "xls: Unknown option '%s'. Try 'xls -H' for help.\n", arg);
            return false;
        } else {
            /* Treat as target directory path */
            MultiByteToWideChar(CP_UTF8, 0, arg, -1, this->targetPath, MAX_PATH_EXT);
        }
    }
    return true;
}

CliOptions* CliOptions_Create(void) {
    CliOptions* obj = (CliOptions*)calloc(1, sizeof(CliOptions));
    if (!obj) return NULL;
    obj->sortBy = SORT_NAME;
    wcscpy_s(obj->targetPath, MAX_PATH_EXT, L".");
    obj->Parse = CliOptions_Parse;
    obj->PrintHelp = CliOptions_PrintHelp;
    return obj;
}

void CliOptions_Destroy(CliOptions* this) {
    if (this) free(this);
}

/* ========================================================================= */
/* 2. VOLUME TELEMETRY CLASS                                                 */
/* ========================================================================= */

typedef struct VolumeInfo {
    wchar_t rootPath[MAX_PATH];
    wchar_t volumeLabel[MAX_PATH];
    wchar_t fileSystem[MAX_PATH];
    DWORD serialNumber;
    DWORD sectorsPerCluster;
    DWORD bytesPerSector;
    DWORD clusterSize;
    uint64_t freeBytes;
    uint64_t totalBytes;

    /* Member Methods */
    bool (*Query)(struct VolumeInfo* this, const wchar_t* path);
    uint64_t (*CalculateAllocation)(const struct VolumeInfo* this, uint64_t fileSize);
} VolumeInfo;

static uint64_t VolumeInfo_CalculateAllocation(const VolumeInfo* this, uint64_t fileSize) {
    if (fileSize == 0 || this->clusterSize == 0) return 0;
    return ((fileSize + this->clusterSize - 1) / this->clusterSize) * this->clusterSize;
}

static bool VolumeInfo_Query(VolumeInfo* this, const wchar_t* path) {
    wchar_t fullPath[MAX_PATH_EXT];
    if (!GetFullPathNameW(path, MAX_PATH_EXT, fullPath, NULL)) {
        return false;
    }

    /* Extract drive root (e.g., C:\) */
    if (fullPath[1] == L':' && fullPath[2] == L'\\') {
        wcsncpy_s(this->rootPath, MAX_PATH, fullPath, 3);
        this->rootPath[3] = L'\0';
    } else {
        wcscpy_s(this->rootPath, MAX_PATH, L"C:\\");
    }

    GetVolumeInformationW(
        this->rootPath,
        this->volumeLabel, MAX_PATH,
        &this->serialNumber,
        NULL, NULL,
        this->fileSystem, MAX_PATH
    );

    DWORD numFreeClusters = 0, totalClusters = 0;
    if (GetDiskFreeSpaceW(this->rootPath,
                          &this->sectorsPerCluster,
                          &this->bytesPerSector,
                          &numFreeClusters,
                          &totalClusters)) {
        this->clusterSize = this->sectorsPerCluster * this->bytesPerSector;
        this->freeBytes = (uint64_t)numFreeClusters * this->clusterSize;
        this->totalBytes = (uint64_t)totalClusters * this->clusterSize;
    } else {
        this->clusterSize = 4096; /* fallback standard cluster */
    }
    return true;
}

VolumeInfo* VolumeInfo_Create(void) {
    VolumeInfo* obj = (VolumeInfo*)calloc(1, sizeof(VolumeInfo));
    if (!obj) return NULL;
    obj->Query = VolumeInfo_Query;
    obj->CalculateAllocation = VolumeInfo_CalculateAllocation;
    return obj;
}

void VolumeInfo_Destroy(VolumeInfo* this) {
    if (this) free(this);
}

/* ========================================================================= */
/* 3. FILE ENTRY CLASS                                                       */
/* ========================================================================= */

typedef struct FileEntry {
    wchar_t name[MAX_PATH];
    wchar_t ext[32];
    uint64_t size;
    uint64_t allocatedSize;
    uint32_t clusterCount;
    DWORD attributes;
    SYSTEMTIME writeTimeLocal;
    bool isDirectory;

    /* Member Methods */
    void (*GetAttributeString)(const struct FileEntry* this, wchar_t* outBuf, size_t maxLen);
    void (*FormatSize)(uint64_t bytes, bool human, wchar_t* outBuf, size_t maxLen);
} FileEntry;

static void FileEntry_GetAttributeString(const FileEntry* this, wchar_t* outBuf, size_t maxLen) {
    /* NTFS: R H S A D C E L */
    swprintf_s(outBuf, maxLen, L"%c%c%c%c%c%c%c%c",
        (this->attributes & FILE_ATTRIBUTE_READONLY)   ? L'R' : L'.',
        (this->attributes & FILE_ATTRIBUTE_HIDDEN)     ? L'H' : L'.',
        (this->attributes & FILE_ATTRIBUTE_SYSTEM)     ? L'S' : L'.',
        (this->attributes & FILE_ATTRIBUTE_ARCHIVE)    ? L'A' : L'.',
        (this->attributes & FILE_ATTRIBUTE_DIRECTORY)  ? L'D' : L'.',
        (this->attributes & FILE_ATTRIBUTE_COMPRESSED) ? L'C' : L'.',
        (this->attributes & FILE_ATTRIBUTE_ENCRYPTED)  ? L'E' : L'.',
        (this->attributes & FILE_ATTRIBUTE_REPARSE_POINT) ? L'L' : L'.'
    );
}

static void FileEntry_FormatSize(uint64_t bytes, bool human, wchar_t* outBuf, size_t maxLen) {
    (void)human; /* sizes are always presented in KB/MB/GB now */
    const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
    int idx = 0;
    double dBytes = (double)bytes;
    while (dBytes >= 1024.0 && idx < 4) {
        dBytes /= 1024.0;
        idx++;
    }
    if (idx == 0) {
        swprintf_s(outBuf, maxLen, L"%llu B", bytes);
    } else {
        swprintf_s(outBuf, maxLen, L"%.1f %ls", dBytes, units[idx]);
    }
}

FileEntry* FileEntry_Create(const WIN32_FIND_DATAW* ffd, const VolumeInfo* vol) {
    FileEntry* obj = (FileEntry*)calloc(1, sizeof(FileEntry));
    if (!obj) return NULL;

    wcsncpy_s(obj->name, MAX_PATH, ffd->cFileName, _TRUNCATE);
    obj->attributes = ffd->dwFileAttributes;
    obj->isDirectory = (ffd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

    /* Extract Extension */
    wchar_t* dot = wcsrchr(obj->name, L'.');
    if (dot && !obj->isDirectory) {
        wcscpy_s(obj->ext, 32, dot + 1);
    } else {
        wcscpy_s(obj->ext, 32, L"");
    }

    /* Size calculation */
    if (obj->isDirectory) {
        obj->size = 0;
        obj->allocatedSize = 0;
        obj->clusterCount = 0;
    } else {
        ULARGE_INTEGER ul;
        ul.LowPart = ffd->nFileSizeLow;
        ul.HighPart = ffd->nFileSizeHigh;
        obj->size = ul.QuadPart;
        obj->allocatedSize = vol->CalculateAllocation(vol, obj->size);
        obj->clusterCount = (vol->clusterSize > 0) ? (uint32_t)(obj->allocatedSize / vol->clusterSize) : 0;
    }

    /* Timestamp Conversion to Local */
    FILETIME ftLocal;
    FileTimeToLocalFileTime(&ffd->ftLastWriteTime, &ftLocal);
    FileTimeToSystemTime(&ftLocal, &obj->writeTimeLocal);

    obj->GetAttributeString = FileEntry_GetAttributeString;
    obj->FormatSize = FileEntry_FormatSize;
    return obj;
}

void FileEntry_Destroy(FileEntry* this) {
    if (this) free(this);
}

/* ========================================================================= */
/* 4. FILE ENTRY COLLECTION CLASS                                            */
/* ========================================================================= */

typedef struct FileCollection {
    FileEntry** items;
    size_t count;
    size_t capacity;

    /* Methods */
    bool (*Add)(struct FileCollection* this, FileEntry* entry);
    void (*Sort)(struct FileCollection* this, SortCriteria criteria, bool reverse);
    void (*Clear)(struct FileCollection* this);
} FileCollection;

static bool FileCollection_Add(FileCollection* this, FileEntry* entry) {
    if (this->count >= this->capacity) {
        size_t newCap = this->capacity == 0 ? 64 : this->capacity * 2;
        FileEntry** newItems = (FileEntry**)realloc(this->items, newCap * sizeof(FileEntry*));
        if (!newItems) return false;
        this->items = newItems;
        this->capacity = newCap;
    }
    this->items[this->count++] = entry;
    return true;
}

/* Sort Comparators */
static int CompName(const void* a, const void* b) {
    const FileEntry* fa = *(const FileEntry**)a;
    const FileEntry* fb = *(const FileEntry**)b;
    if (fa->isDirectory != fb->isDirectory) return fa->isDirectory ? -1 : 1;
    return _wcsicmp(fa->name, fb->name);
}

static int CompExt(const void* a, const void* b) {
    const FileEntry* fa = *(const FileEntry**)a;
    const FileEntry* fb = *(const FileEntry**)b;
    if (fa->isDirectory != fb->isDirectory) return fa->isDirectory ? -1 : 1;
    int cmp = _wcsicmp(fa->ext, fb->ext);
    return cmp != 0 ? cmp : _wcsicmp(fa->name, fb->name);
}

static int CompSize(const void* a, const void* b) {
    const FileEntry* fa = *(const FileEntry**)a;
    const FileEntry* fb = *(const FileEntry**)b;
    if (fa->isDirectory != fb->isDirectory) return fa->isDirectory ? -1 : 1;
    if (fa->size < fb->size) return -1;
    if (fa->size > fb->size) return 1;
    return _wcsicmp(fa->name, fb->name);
}

static int CompDate(const void* a, const void* b) {
    const FileEntry* fa = *(const FileEntry**)a;
    const FileEntry* fb = *(const FileEntry**)b;
    if (fa->isDirectory != fb->isDirectory) return fa->isDirectory ? -1 : 1;
    FILETIME fta, ftb;
    SystemTimeToFileTime(&fa->writeTimeLocal, &fta);
    SystemTimeToFileTime(&fb->writeTimeLocal, &ftb);
    return CompareFileTime(&fta, &ftb);
}

static void FileCollection_Sort(FileCollection* this, SortCriteria criteria, bool reverse) {
    int (*comparator)(const void*, const void*) = CompName;
    switch (criteria) {
        case SORT_EXT:  comparator = CompExt;  break;
        case SORT_SIZE: comparator = CompSize; break;
        case SORT_DATE: comparator = CompDate; break;
        case SORT_NAME:
        default:        comparator = CompName; break;
    }
    qsort(this->items, this->count, sizeof(FileEntry*), comparator);

    if (reverse) {
        for (size_t i = 0; i < this->count / 2; ++i) {
            FileEntry* tmp = this->items[i];
            this->items[i] = this->items[this->count - 1 - i];
            this->items[this->count - 1 - i] = tmp;
        }
    }
}

static void FileCollection_Clear(FileCollection* this) {
    for (size_t i = 0; i < this->count; ++i) {
        FileEntry_Destroy(this->items[i]);
    }
    free(this->items);
    this->items = NULL;
    this->count = 0;
    this->capacity = 0;
}

FileCollection* FileCollection_Create(void) {
    FileCollection* obj = (FileCollection*)calloc(1, sizeof(FileCollection));
    if (!obj) return NULL;
    obj->Add = FileCollection_Add;
    obj->Sort = FileCollection_Sort;
    obj->Clear = FileCollection_Clear;
    return obj;
}

void FileCollection_Destroy(FileCollection* this) {
    if (this) {
        this->Clear(this);
        free(this);
    }
}

/* ========================================================================= */
/* 5. XLS PRESENTATION FORMATTER CLASS                               */
/* ========================================================================= */

typedef struct XDirFormatter {
    const CliOptions* opts;
    const VolumeInfo* vol;

    /* Methods */
    void (*PrintHeader)(const struct XDirFormatter* this, const wchar_t* searchDir);
    void (*PrintRow)(const struct XDirFormatter* this, const FileEntry* entry);
    void (*PrintSummary)(const struct XDirFormatter* this, size_t fileCount, size_t dirCount,
                         uint64_t totalBytes, uint64_t totalAllocated);
} XDirFormatter;

static void XDirFormatter_PrintHeader(const XDirFormatter* this, const wchar_t* searchDir) {
    bool color = !this->opts->noColor;
    const wchar_t* cHeader = color ? L"\x1b[1;37m" : L"";
    const wchar_t* cReset  = color ? L"\x1b[0m" : L"";

    wprintf(L"%lsVolume in drive %ls is %ls%ls (Serial: %04X-%04X, FS: %ls, Cluster: %u bytes)\n",
        cHeader,
        this->vol->rootPath,
        wcslen(this->vol->volumeLabel) ? this->vol->volumeLabel : L"NO_LABEL",
        cReset,
        HIWORD(this->vol->serialNumber), LOWORD(this->vol->serialNumber),
        this->vol->fileSystem,
        this->vol->clusterSize
    );
    wprintf(L"Directory of %ls%ls%ls\n\n", cHeader, searchDir, cReset);

    /* Column Headers */
    if (this->opts->longListing) {
        if (this->opts->showClusters) {
            wprintf(L"Attrib    Date        Time        Clusters        Allocated             Size  Name\n");
            wprintf(L"--------  ----------  --------  ----------  ---------------  ---------------  ------------------------\n");
        } else {
            wprintf(L"Attrib    Date        Time              Allocated             Size  Name\n");
            wprintf(L"--------  ----------  --------  -----------------  ---------------  ------------------------\n");
        }
    }
}

static void XDirFormatter_PrintRow(const XDirFormatter* this, const FileEntry* entry) {
    bool color = !this->opts->noColor;
    const wchar_t* cDir   = color ? L"\x1b[1;34m" : L"";
    const wchar_t* cExe   = color ? L"\x1b[1;32m" : L"";
    const wchar_t* cSys   = color ? L"\x1b[1;33m" : L"";
    const wchar_t* cReset = color ? L"\x1b[0m" : L"";

    wchar_t attrStr[16];
    entry->GetAttributeString(entry, attrStr, 16);

    wchar_t szStr[32], allocStr[32];
    if (entry->isDirectory) {
        wcscpy_s(szStr, 32, L"<DIR>");
        wcscpy_s(allocStr, 32, L"");
    } else {
        FileEntry_FormatSize(entry->size, this->opts->humanReadable, szStr, 32);
        FileEntry_FormatSize(entry->allocatedSize, this->opts->humanReadable, allocStr, 32);
    }

    /* Color Selection */
    const wchar_t* nameColor = cReset;
    if (entry->isDirectory) {
        nameColor = cDir;
    } else if (_wcsicmp(entry->ext, L"exe") == 0 || _wcsicmp(entry->ext, L"com") == 0 ||
               _wcsicmp(entry->ext, L"bat") == 0 || _wcsicmp(entry->ext, L"cmd") == 0) {
        nameColor = cExe;
    } else if (entry->attributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) {
        nameColor = cSys;
    }

    if (this->opts->longListing) {
        if (this->opts->showClusters) {
            if (entry->isDirectory) {
                wprintf(L"%-8ls  %02d-%02d-%04d  %02d:%02d:%02d          --               --  %15ls  %ls%ls%ls\n",
                    attrStr,
                    entry->writeTimeLocal.wMonth, entry->writeTimeLocal.wDay, entry->writeTimeLocal.wYear,
                    entry->writeTimeLocal.wHour, entry->writeTimeLocal.wMinute, entry->writeTimeLocal.wSecond,
                    szStr, nameColor, entry->name, cReset);
            } else {
                wprintf(L"%-8ls  %02d-%02d-%04d  %02d:%02d:%02d  %10u  %15ls  %15ls  %ls%ls%ls\n",
                    attrStr,
                    entry->writeTimeLocal.wMonth, entry->writeTimeLocal.wDay, entry->writeTimeLocal.wYear,
                    entry->writeTimeLocal.wHour, entry->writeTimeLocal.wMinute, entry->writeTimeLocal.wSecond,
                    entry->clusterCount, allocStr, szStr, nameColor, entry->name, cReset);
            }
        } else {
            if (entry->isDirectory) {
                wprintf(L"%-8ls  %02d-%02d-%04d  %02d:%02d:%02d                 --  %15ls  %ls%ls%ls\n",
                    attrStr,
                    entry->writeTimeLocal.wMonth, entry->writeTimeLocal.wDay, entry->writeTimeLocal.wYear,
                    entry->writeTimeLocal.wHour, entry->writeTimeLocal.wMinute, entry->writeTimeLocal.wSecond,
                    szStr, nameColor, entry->name, cReset);
            } else {
                wprintf(L"%-8ls  %02d-%02d-%04d  %02d:%02d:%02d  %17ls  %15ls  %ls%ls%ls\n",
                    attrStr,
                    entry->writeTimeLocal.wMonth, entry->writeTimeLocal.wDay, entry->writeTimeLocal.wYear,
                    entry->writeTimeLocal.wHour, entry->writeTimeLocal.wMinute, entry->writeTimeLocal.wSecond,
                    allocStr, szStr, nameColor, entry->name, cReset);
            }
        }
    } else {
        /* Standard Compact XLS 2-column view */
        wprintf(L"%-32ls  %12ls\n", entry->name, szStr);
    }
}

static void XDirFormatter_PrintSummary(const XDirFormatter* this, size_t fileCount, size_t dirCount,
                                       uint64_t totalBytes, uint64_t totalAllocated) {
    wchar_t bytesStr[32], allocStr[32], freeStr[32];
    FileEntry_FormatSize(totalBytes, this->opts->humanReadable, bytesStr, 32);
    FileEntry_FormatSize(totalAllocated, this->opts->humanReadable, allocStr, 32);
    FileEntry_FormatSize(this->vol->freeBytes, this->opts->humanReadable, freeStr, 32);

    int64_t slack = (int64_t)totalAllocated - (int64_t)totalBytes;
    if (slack < 0) slack = 0;
    wchar_t slackStr[32];
    FileEntry_FormatSize((uint64_t)slack, this->opts->humanReadable, slackStr, 32);

    wprintf(L"----------------------------------------------------------------------------------------\n");
    wprintf(L" Total Files : %-8zu  Payload: %ls (Allocated: %ls, Slack: %ls)\n",
        fileCount, bytesStr, allocStr, slackStr);
    wprintf(L" Total Dirs  : %-8zu  Volume Free Space: %ls\n\n",
        dirCount, freeStr);
}

XDirFormatter* XDirFormatter_Create(const CliOptions* opts, const VolumeInfo* vol) {
    XDirFormatter* obj = (XDirFormatter*)calloc(1, sizeof(XDirFormatter));
    if (!obj) return NULL;
    obj->opts = opts;
    obj->vol = vol;
    obj->PrintHeader = XDirFormatter_PrintHeader;
    obj->PrintRow = XDirFormatter_PrintRow;
    obj->PrintSummary = XDirFormatter_PrintSummary;
    return obj;
}

void XDirFormatter_Destroy(XDirFormatter* this) {
    if (this) free(this);
}

/* ========================================================================= */
/* 6. SCAN ENGINE CLASS                                                      */
/* ========================================================================= */

typedef struct ScanEngine {
    const CliOptions* opts;
    const VolumeInfo* vol;
    XDirFormatter* formatter;

    /* Methods */
    void (*ProcessPath)(struct ScanEngine* this, const wchar_t* path);
} ScanEngine;

static void ScanEngine_ProcessPath(ScanEngine* this, const wchar_t* path) {
    wchar_t searchPattern[MAX_PATH_EXT];
    wchar_t resolvedDir[MAX_PATH_EXT];

    if (!GetFullPathNameW(path, MAX_PATH_EXT, resolvedDir, NULL)) {
        wprintf(L"xls: Cannot resolve path '%ls': %lu\n", path, GetLastError());
        return;
    }

    /* Strip trailing slashes */
    size_t len = wcslen(resolvedDir);
    if (len > 3 && (resolvedDir[len - 1] == L'\\' || resolvedDir[len - 1] == L'/')) {
        resolvedDir[len - 1] = L'\0';
    }

    swprintf_s(searchPattern, MAX_PATH_EXT, L"%ls\\*", resolvedDir);

    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(searchPattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        wprintf(L"xls: Cannot access path '%ls': %lu\n", resolvedDir, GetLastError());
        return;
    }

    FileCollection* collection = FileCollection_Create();
    FileCollection* subdirs = FileCollection_Create();

    size_t fileCount = 0;
    size_t dirCount = 0;
    uint64_t totalBytes = 0;
    uint64_t totalAllocated = 0;

    do {
        /* Skip self and parent directory pointers */
        if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) {
            continue;
        }

        /* Hidden/System filtering */
        bool isHidden = (ffd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0;
        if (isHidden && !this->opts->showAll) {
            continue;
        }

        FileEntry* entry = FileEntry_Create(&ffd, this->vol);
        if (!entry) continue;
        if (!collection->Add(collection, entry)) {
            FileEntry_Destroy(entry);
            continue;
        }

        if (entry->isDirectory) {
            dirCount++;
            if (this->opts->recursive) {
                FileEntry* subEntry = FileEntry_Create(&ffd, this->vol);
                if (subEntry) {
                    if (!subdirs->Add(subdirs, subEntry)) {
                        FileEntry_Destroy(subEntry);
                    }
                }
            }
        } else {
            fileCount++;
            totalBytes += entry->size;
            totalAllocated += entry->allocatedSize;
        }

    } while (FindNextFileW(hFind, &ffd));
    FindClose(hFind);

    /* Sort entries according to options */
    collection->Sort(collection, this->opts->sortBy, this->opts->reverseSort);

    /* Output Presentation */
    this->formatter->PrintHeader(this->formatter, resolvedDir);
    for (size_t i = 0; i < collection->count; ++i) {
        this->formatter->PrintRow(this->formatter, collection->items[i]);
    }
    this->formatter->PrintSummary(this->formatter, fileCount, dirCount, totalBytes, totalAllocated);

    FileCollection_Destroy(collection);

    /* Recurse into children if requested */
    if (this->opts->recursive) {
        for (size_t i = 0; i < subdirs->count; ++i) {
            wchar_t nextDir[MAX_PATH_EXT];
            swprintf_s(nextDir, MAX_PATH_EXT, L"%ls\\%ls", resolvedDir, subdirs->items[i]->name);
            this->ProcessPath(this, nextDir);
        }
    }
    FileCollection_Destroy(subdirs);
}

ScanEngine* ScanEngine_Create(const CliOptions* opts, const VolumeInfo* vol, XDirFormatter* formatter) {
    ScanEngine* obj = (ScanEngine*)calloc(1, sizeof(ScanEngine));
    if (!obj) return NULL;
    obj->opts = opts;
    obj->vol = vol;
    obj->formatter = formatter;
    obj->ProcessPath = ScanEngine_ProcessPath;
    return obj;
}

void ScanEngine_Destroy(ScanEngine* this) {
    if (this) free(this);
}

/* ========================================================================= */
/* 7. APPLICATION CONTROLLER                                                 */
/* ========================================================================= */

typedef struct Application {
    CliOptions* options;
    VolumeInfo* volume;
    XDirFormatter* formatter;
    ScanEngine* engine;

    /* Method */
    int (*Run)(struct Application* this, int argc, char** argv);
} Application;

static bool SetupAnsiConsole(DWORD* outOrigMode) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE || hOut == NULL) return false;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return false; /* not a console (redirected) */
    if (outOrigMode) *outOrigMode = dwMode;
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    return true;
}

static void RestoreAnsiConsole(bool wasConsole, DWORD origMode) {
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
        this->options->PrintHelp(this->options);
        return EXIT_SUCCESS;
    }

    DWORD origMode = 0;
    bool isConsole = false;
    if (!this->options->noColor) {
        isConsole = SetupAnsiConsole(&origMode);
        if (!isConsole) {
            /* Redirected to a file/pipe: suppress color so no ANSI leaks */
            this->options->noColor = true;
        }
    }

    if (!this->volume->Query(this->volume, this->options->targetPath)) {
        fprintf(stderr, "xls: Failed to query volume characteristics.\n");
        return EXIT_FAILURE;
    }

    this->formatter = XDirFormatter_Create(this->options, this->volume);
    this->engine = ScanEngine_Create(this->options, this->volume, this->formatter);

    this->engine->ProcessPath(this->engine, this->options->targetPath);

    RestoreAnsiConsole(isConsole, origMode);

    return EXIT_SUCCESS;
}

Application* Application_Create(void) {
    Application* app = (Application*)calloc(1, sizeof(Application));
    if (!app) return NULL;
    app->options = CliOptions_Create();
    app->volume = VolumeInfo_Create();
    app->Run = Application_Run;
    return app;
}

void Application_Destroy(Application* this) {
    if (!this) return;
    if (this->engine) ScanEngine_Destroy(this->engine);
    if (this->formatter) XDirFormatter_Destroy(this->formatter);
    if (this->volume) VolumeInfo_Destroy(this->volume);
    if (this->options) CliOptions_Destroy(this->options);
    free(this);
}

/* ========================================================================= */
/* 8. ENTRY POINT                                                            */
/* ========================================================================= */

int main(int argc, char** argv) {
    Application* app = Application_Create();
    if (!app) {
        fprintf(stderr, "xls: Out of memory.\n");
        return EXIT_FAILURE;
    }

    int exitCode = app->Run(app, argc, argv);
    Application_Destroy(app);

    return exitCode;
}