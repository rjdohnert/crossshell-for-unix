#include "cli.h"

static void CliOptions_AppendFile(CliOptions* self, const wchar_t* file) {
    if (self->fileCount >= self->fileCapacity) {
        self->fileCapacity = (self->fileCapacity == 0) ? 16 : self->fileCapacity * 2;
        self->fileList = (wchar_t**)realloc(self->fileList, (size_t)self->fileCapacity * sizeof(wchar_t*));
    }
    self->fileList[self->fileCount++] = _wcsdup(file);
}

static void CliOptions_ExpandWildcards(CliOptions* self, const wchar_t* pattern) {
    if (wcspbrk(pattern, L"*?") == NULL) {
        CliOptions_AppendFile(self, pattern);
        return;
    }

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(pattern, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        CliOptions_AppendFile(self, pattern);
        return;
    }

    wchar_t dir[PATH_BUFFER_CAPACITY];
    wcsncpy_s(dir, PATH_BUFFER_CAPACITY, pattern, _TRUNCATE);
    wchar_t* lastSlash = wcsrchr(dir, L'\\');
    if (!lastSlash) lastSlash = wcsrchr(dir, L'/');
    if (lastSlash) *(lastSlash + 1) = L'\0'; else dir[0] = L'\0';

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            wchar_t fullPath[PATH_BUFFER_CAPACITY];
            swprintf_s(fullPath, PATH_BUFFER_CAPACITY, L"%ls%ls", dir, findData.cFileName);
            CliOptions_AppendFile(self, fullPath);
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
}

static void ShowHelp(void) {
    wprintf(
        L"Usage: uncompress [options] [file ...]\n"
        L"Uncompress files compressed using Lempel-Ziv-Welch (.Z) encoding.\n\n"
        L"Options:\n"
        L"  -c, --stdout      Write decompressed data to standard output.\n"
        L"  -f, --force       Force overwrite of destination files without prompting.\n"
        L"  -k, --keep        Retain source .Z archive after successful decompression.\n"
        L"  -q, --quiet       Quiet mode; suppress all non-critical warnings.\n"
        L"  -v, --verbose     Verbose mode; print compression ratios and file replacements.\n"
        L"  -V, --version     Display application version information.\n"
        L"  -h, --help        Display this help screen.\n"
        L"  --                Terminate option scanning.\n\n"
        L"Compatibility:\n"
        L"  Supports classic UNIX compress 4.x files (9-16 bits, variable length codes).\n"
        L"  Safe against circular prefix references and buffer overflows (CVE-2006-1168).\n"
    );
}

CliOptions* CliOptions_Create(int argc, wchar_t* argv[]) {
    CliOptions* self = (CliOptions*)calloc(1, sizeof(CliOptions));
    if (!self) return NULL;

    bool endOfFlags = false;
    for (int i = 1; i < argc; ++i) {
        wchar_t* arg = argv[i];
        if (!endOfFlags && arg[0] == L'-' && arg[1] != L'\0') {
            if (wcscmp(arg, L"--") == 0) {
                endOfFlags = true;
                continue;
            }
            if (wcscmp(arg, L"-h") == 0 || wcscmp(arg, L"--help") == 0) {
                ShowHelp();
                exit(0);
            }
            if (wcscmp(arg, L"-V") == 0 || wcscmp(arg, L"--version") == 0) {
                wprintf(L"uncompress %ls\n", UNCOMPRESS_VERSION);
                exit(0);
            }
            for (size_t j = 1; arg[j] != L'\0'; ++j) {
                switch (arg[j]) {
                    case L'c': self->toStdout = true; break;
                    case L'f': self->force = true; break;
                    case L'k': self->keep = true; break;
                    case L'v': self->verbose = true; break;
                    case L'q': self->quiet = true; break;
                    default:
                        fwprintf(stderr, L"uncompress: unknown option '-%c'\n", arg[j]);
                        fwprintf(stderr, L"Try 'uncompress --help' for more options.\n");
                        exit(1);
                }
            }
        } else {
            CliOptions_ExpandWildcards(self, arg);
        }
    }
    return self;
}

void CliOptions_Destroy(CliOptions* self) {
    if (!self) return;
    for (int i = 0; i < self->fileCount; ++i) {
        free(self->fileList[i]);
    }
    free(self->fileList);
    free(self);
}
