#define UNICODE
#define _UNICODE

#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <wchar.h>
#include <io.h>
#include <fcntl.h>

#define CP_VERSION L"1.2.0"

// Wide buffers for long-path (\\?\) support
#define CP_PATH 32767

// Prefix a path with \\?\ so Win32 APIs accept paths longer than MAX_PATH.
// Returns a pointer to a static buffer (or the original path if short).
// Not thread-safe; cp is single-threaded.
static const wchar_t *long_path(const wchar_t *path) {
    static wchar_t buf[CP_PATH];
    size_t len = wcslen(path);
    if (len < MAX_PATH || wcsncmp(path, L"\\\\?\\", 4) == 0) {
        return path;
    }
    // GetFullPathNameW needs a buffer large enough for the result
    static wchar_t full[CP_PATH];
    DWORD n = GetFullPathNameW(path, CP_PATH, full, NULL);
    if (n == 0 || n >= CP_PATH) {
        return path;
    }
    // UNC paths (\\server\share) need the \\?\UNC\ form
    if (wcsncmp(full, L"\\\\", 2) == 0) {
        swprintf(buf, CP_PATH, L"\\\\?\\UNC\\%ls", full + 2);
    } else {
        swprintf(buf, CP_PATH, L"\\\\?\\%ls", full);
    }
    return buf;
}

// Print a Win32 error as human-readable text
static void print_error(const wchar_t *prefix, const wchar_t *path, DWORD err) {
    wchar_t msg[512];
    DWORD n = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL, err, 0, msg, 512, NULL);
    if (n > 0) {
        // Trim trailing CRLF from the system message
        while (n > 0 && (msg[n - 1] == L'\r' || msg[n - 1] == L'\n')) msg[--n] = L'\0';
        wprintf(L"cp: %ls '%ls': %ls\n", prefix, path, msg);
    } else {
        wprintf(L"cp: %ls '%ls': Error code %lu\n", prefix, path, err);
    }
}

typedef struct {
    bool recursive;       // -r, -R
    bool force;           // -f
    bool interactive;     // -i
    bool verbose;         // -v
    bool no_clobber;      // -n
    bool update;          // -u
    bool preserve;        // -p
} Options;

void print_help(void) {
    wprintf(L"Usage: cp [OPTION]... SOURCE... DIRECTORY\n");
    wprintf(L"   or: cp [OPTION]... SOURCE DEST\n");
    wprintf(L"Copy SOURCE to DEST, or multiple SOURCE(s) to DIRECTORY.\n\n");
    wprintf(L"Mandatory arguments to long options are mandatory for short options too.\n");
    wprintf(L"  -f, --force          if an existing destination file cannot be\n");
    wprintf(L"                         opened, remove it and try again\n");
    wprintf(L"  -i, --interactive    prompt before overwrite (overrides -n)\n");
    wprintf(L"  -n, --no-clobber     do not overwrite an existing file (overrides -i)\n");
    wprintf(L"  -p, --preserve       preserve file attributes and timestamps\n");
    wprintf(L"  -r, -R, --recursive  copy directories recursively\n");
    wprintf(L"  -u, --update         copy only when the SOURCE file is newer than\n");
    wprintf(L"                         the destination file or when missing\n");
    wprintf(L"  -v, --verbose        explain what is being done\n");
    wprintf(L"      --help           display this help and exit\n");
    wprintf(L"      --version        output version information and exit\n\n");
    wprintf(L"Windows build notes:\n");
    wprintf(L"  Forward slashes (/) and backslashes (\\) are both accepted as path separators.\n");
}

void print_version(void) {
    wprintf(L"cp (CrossShell) version %ls\n", CP_VERSION);
    wprintf(L"Copyright (C) 2026, PC/OpenSystems LLC.\n");
}

bool path_exists(const wchar_t *path) {
    return GetFileAttributesW(long_path(path)) != INVALID_FILE_ATTRIBUTES;
}

bool is_directory(const wchar_t *path) {
    DWORD attr = GetFileAttributesW(long_path(path));
    return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

// Check if source file is newer than destination
bool is_source_newer(const wchar_t *src, const wchar_t *dst) {
    WIN32_FILE_ATTRIBUTE_DATA src_data, dst_data;
    if (!GetFileAttributesExW(long_path(src), GetFileExInfoStandard, &src_data)) return true;
    if (!GetFileAttributesExW(long_path(dst), GetFileExInfoStandard, &dst_data)) return true;

    return CompareFileTime(&src_data.ftLastWriteTime, &dst_data.ftLastWriteTime) > 0;
}

// Ask user before overwriting
bool confirm_overwrite(const wchar_t *dest) {
    wprintf(L"cp: overwrite '%ls'? (y/n [n]) ", dest);
    wchar_t response[16];
    if (fgetws(response, 16, stdin) != NULL) {
        return (response[0] == L'y' || response[0] == L'Y');
    }
    return false;
}

// Extract base name from a path (e.g. "foo/bar/baz.txt" -> "baz.txt")
const wchar_t *get_filename(const wchar_t *path) {
    const wchar_t *slash = wcsrchr(path, L'/');
    const wchar_t *backslash = wcsrchr(path, L'\\');
    const wchar_t *last = slash > backslash ? slash : backslash;
    return last ? (last + 1) : path;
}

// Copy a single file with Win32 APIs
bool copy_file(const wchar_t *src, const wchar_t *dst, const Options *opt) {
    // long_path uses a static buffer, so copy the src result before resolving dst
    static wchar_t src_lp[CP_PATH];
    wcscpy_s(src_lp, CP_PATH, long_path(src));
    const wchar_t *src_api = src_lp;
    const wchar_t *dst_api = long_path(dst);

    // Refuse to copy a file onto itself
    wchar_t src_full[MAX_PATH], dst_full[MAX_PATH];
    if (GetFullPathNameW(src, MAX_PATH, src_full, NULL) &&
        GetFullPathNameW(dst, MAX_PATH, dst_full, NULL) &&
        _wcsicmp(src_full, dst_full) == 0) {
        wprintf(L"cp: '%ls' and '%ls' are the same file\n", src, dst);
        return false;
    }

    bool dest_exists = path_exists(dst);

    if (dest_exists) {
        if (opt->no_clobber) {
            return true; // Skip silently
        }

        if (opt->update && !is_source_newer(src, dst)) {
            return true; // Target is up to date
        }

        if (opt->interactive && !confirm_overwrite(dst)) {
            return true; // Skipped by user
        }

        if (opt->force) {
            // Remove Read-Only attribute if present so we can overwrite
            DWORD attr = GetFileAttributesW(dst_api);
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY)) {
                SetFileAttributesW(dst_api, attr & ~FILE_ATTRIBUTE_READONLY);
            }
        }
    }

    // CopyFileW performs fast kernel-level streaming copy
    bool copied = CopyFileW(src_api, dst_api, FALSE) != 0;
    if (!copied && opt->force && dest_exists) {
        // GNU -f: if the destination cannot be opened, remove it and try again
        if (DeleteFileW(dst_api)) {
            copied = CopyFileW(src_api, dst_api, FALSE) != 0;
        }
    }
    if (!copied) {
        print_error(L"cannot create regular file", dst, GetLastError());
        return false;
    }

    if (opt->preserve) {
        // Preserve original timestamps
        HANDLE hSrc = CreateFileW(src_api, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (hSrc != INVALID_HANDLE_VALUE) {
            FILETIME ftCreate, ftAccess, ftWrite;
            if (GetFileTime(hSrc, &ftCreate, &ftAccess, &ftWrite)) {
                HANDLE hDst = CreateFileW(dst_api, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
                if (hDst != INVALID_HANDLE_VALUE) {
                    SetFileTime(hDst, &ftCreate, &ftAccess, &ftWrite);
                    CloseHandle(hDst);
                }
            }
            CloseHandle(hSrc);
        }

        // Preserve original attributes
        DWORD attr = GetFileAttributesW(src_api);
        if (attr != INVALID_FILE_ATTRIBUTES) {
            SetFileAttributesW(dst_api, attr);
        }
    }

    if (opt->verbose) {
        wprintf(L"'%ls' -> '%ls'\n", src, dst);
    }

    return true;
}

// Copy directory recursively
bool copy_directory(const wchar_t *src, const wchar_t *dst, const Options *opt) {
    if (!opt->recursive) {
        wprintf(L"cp: -r not specified; omitting directory '%ls'\n", src);
        return false;
    }

    // long_path uses a static buffer, so copy the src result before resolving dst
    static wchar_t src_lp[CP_PATH];
    wcscpy_s(src_lp, CP_PATH, long_path(src));
    const wchar_t *src_api = src_lp;
    const wchar_t *dst_api = long_path(dst);

    // Refuse to copy a directory into itself (would recurse until path truncation)
    wchar_t src_full[MAX_PATH], dst_full[MAX_PATH];
    if (GetFullPathNameW(src, MAX_PATH, src_full, NULL) &&
        GetFullPathNameW(dst, MAX_PATH, dst_full, NULL)) {
        size_t n = wcslen(src_full);
        if (_wcsnicmp(src_full, dst_full, n) == 0 &&
            (dst_full[n] == L'\0' || dst_full[n] == L'\\' || dst_full[n] == L'/')) {
            wprintf(L"cp: cannot copy a directory, '%ls', into itself, '%ls'\n", src, dst);
            return false;
        }
    }

    // Create target directory
    if (!CreateDirectoryW(dst_api, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        print_error(L"cannot create directory", dst, GetLastError());
        return false;
    }

    if (opt->verbose) {
        wprintf(L"'%ls' => '%ls' (directory created)\n", src, dst);
    }

    static wchar_t search_path[CP_PATH];
    swprintf(search_path, CP_PATH, L"%ls\\*", src_api);

    WIN32_FIND_DATAW find_data;
    HANDLE hFind = FindFirstFileW(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) {
        print_error(L"cannot read directory", src, GetLastError());
        return false;
    }

    bool success = true;

    do {
        // Skip "." and ".."
        if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0) {
            continue;
        }

        static wchar_t sub_src[CP_PATH];
        static wchar_t sub_dst[CP_PATH];
        swprintf(sub_src, CP_PATH, L"%ls\\%ls", src, find_data.cFileName);
        swprintf(sub_dst, CP_PATH, L"%ls\\%ls", dst, find_data.cFileName);

        // Skip reparse points (junctions/symlinks) to avoid copy cycles
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            wprintf(L"cp: skipping reparse point '%ls'\n", sub_src);
            continue;
        }

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!copy_directory(sub_src, sub_dst, opt)) {
                success = false;
            }
        } else {
            if (!copy_file(sub_src, sub_dst, opt)) {
                success = false;
            }
        }
    } while (FindNextFileW(hFind, &find_data));

    FindClose(hFind);

    // Preserve directory timestamps (must happen after children are written,
    // since creating files inside updates the directory's mtime)
    if (opt->preserve) {
        HANDLE hSrc = CreateFileW(src_api, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (hSrc != INVALID_HANDLE_VALUE) {
            FILETIME ftCreate, ftAccess, ftWrite;
            if (GetFileTime(hSrc, &ftCreate, &ftAccess, &ftWrite)) {
                HANDLE hDst = CreateFileW(dst_api, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                          NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
                if (hDst != INVALID_HANDLE_VALUE) {
                    SetFileTime(hDst, &ftCreate, &ftAccess, &ftWrite);
                    CloseHandle(hDst);
                }
            }
            CloseHandle(hSrc);
        }
    }

    return success;
}

int wmain(int argc, wchar_t *argv[]) {
    // Enable UTF-16 console output so non-ASCII filenames render correctly
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    Options opt = {0};
    int arg_index = 1;

    // Parse options
    while (arg_index < argc && argv[arg_index][0] == L'-' && argv[arg_index][1] != L'\0') {
        wchar_t *arg = argv[arg_index];

        if (wcscmp(arg, L"--") == 0) {
            arg_index++;
            break;
        }
        if (wcscmp(arg, L"--help") == 0) {
            print_help();
            return 0;
        }
        if (wcscmp(arg, L"--version") == 0) {
            print_version();
            return 0;
        }
        if (wcscmp(arg, L"--recursive") == 0) {
            opt.recursive = true;
        } else if (wcscmp(arg, L"--force") == 0) {
            opt.force = true;
        } else if (wcscmp(arg, L"--interactive") == 0) {
            opt.interactive = true;
            opt.no_clobber = false;
        } else if (wcscmp(arg, L"--no-clobber") == 0) {
            opt.no_clobber = true;
            opt.interactive = false;
        } else if (wcscmp(arg, L"--update") == 0) {
            opt.update = true;
        } else if (wcscmp(arg, L"--preserve") == 0) {
            opt.preserve = true;
        } else if (wcscmp(arg, L"--verbose") == 0) {
            opt.verbose = true;
        } else if (arg[1] != L'-') {
            // Short switches (supports chaining e.g. -rvf)
            for (size_t j = 1; j < wcslen(arg); j++) {
                switch (arg[j]) {
                    case L'r': case L'R': opt.recursive = true; break;
                    case L'f': opt.force = true; break;
                    case L'i': opt.interactive = true; opt.no_clobber = false; break;
                    case L'n': opt.no_clobber = true; opt.interactive = false; break;
                    case L'u': opt.update = true; break;
                    case L'p': opt.preserve = true; break;
                    case L'v': opt.verbose = true; break;
                    case L'h': print_help(); return 0;
                    default:
                        wprintf(L"cp: invalid option -- '%c'\n", arg[j]);
                        wprintf(L"Try 'cp --help' for more information.\n");
                        return 1;
                }
            }
        } else {
            wprintf(L"cp: unrecognized option '%ls'\n", arg);
            wprintf(L"Try 'cp --help' for more information.\n");
            return 1;
        }
        arg_index++;
    }

    int remaining = argc - arg_index;
    if (remaining < 1) {
        wprintf(L"cp: missing file operand\n");
        wprintf(L"Try 'cp --help' for more information.\n");
        return 1;
    }
    if (remaining < 2) {
        wprintf(L"cp: missing destination file operand after '%ls'\n", argv[arg_index]);
        wprintf(L"Try 'cp --help' for more information.\n");
        return 1;
    }

    wchar_t *target = argv[argc - 1];
    bool target_is_dir = is_directory(target);

    // If more than 2 positional paths, destination must be a directory
    if (remaining > 2 && !target_is_dir) {
        wprintf(L"cp: target '%ls' is not a directory\n", target);
        return 1;
    }

    int exit_code = 0;

    for (int i = arg_index; i < argc - 1; i++) {
        const wchar_t *src = argv[i];

        // Strip trailing path separators so the base name resolves correctly
        wchar_t src_buffer[CP_PATH];
        wcscpy_s(src_buffer, CP_PATH, src);
        size_t src_len = wcslen(src_buffer);
        while (src_len > 1 && (src_buffer[src_len - 1] == L'/' || src_buffer[src_len - 1] == L'\\')) {
            src_buffer[--src_len] = L'\0';
        }
        src = src_buffer;

        if (!path_exists(src)) {
            wprintf(L"cp: cannot stat '%ls': No such file or directory\n", src);
            exit_code = 1;
            continue;
        }

        wchar_t dest_buffer[CP_PATH];
        if (target_is_dir) {
            swprintf(dest_buffer, CP_PATH, L"%ls\\%ls", target, get_filename(src));
        } else {
            wcscpy_s(dest_buffer, CP_PATH, target);
        }

        if (is_directory(src)) {
            if (!copy_directory(src, dest_buffer, &opt)) {
                exit_code = 1;
            }
        } else {
            if (!copy_file(src, dest_buffer, &opt)) {
                exit_code = 1;
            }
        }
    }

    return exit_code;
}