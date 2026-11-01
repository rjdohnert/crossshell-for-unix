#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <wchar.h>
#include <conio.h>
#include <time.h>

#define FSCTL_VERSION "1.2.0"
#define MAX_ENTRIES 4096

typedef enum {
    SORT_NAME = 0,
    SORT_SIZE,
    SORT_DATE
} SortMode;

typedef struct {
    wchar_t name[MAX_PATH];
    bool is_dir;
    bool is_hidden;
    unsigned long long size;
    FILETIME mtime;
} FileEntry;

typedef struct {
    wchar_t current_path[MAX_PATH];
    FileEntry entries[MAX_ENTRIES];
    int count;
    int selected;
    int scroll_offset;
    
    // Qualifiers / Settings
    bool show_hidden;
    SortMode sort_mode;
    bool reverse_sort;
    bool readonly_mode;
    bool show_help;
    
    // Console dimensions
    int term_width;
    int term_height;
    
    // Win32 Console State
    HANDLE h_out;
    HANDLE h_in;
    DWORD orig_out_mode;
    DWORD orig_in_mode;
} AppState;

static AppState g_app;

// --- Forward Declarations ---
void init_console(void);
void restore_console(void);
void load_directory(void);
void render_ui(void);
void handle_input(void);
void sort_entries(void);
void print_cli_help(const wchar_t *prog_name);

// --- Formatting Helpers ---
void format_size(unsigned long long bytes, wchar_t *out, size_t out_sz) {
    const wchar_t *units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
    int unit_idx = 0;
    double d_size = (double)bytes;

    while (d_size >= 1024.0 && unit_idx < 4) {
        d_size /= 1024.0;
        unit_idx++;
    }
    if (unit_idx == 0) {
        swprintf(out, out_sz, L"%llu B", bytes);
    } else {
        swprintf(out, out_sz, L"%.1f %ls", d_size, units[unit_idx]);
    }
}

void format_filetime(FILETIME ft, wchar_t *out, size_t out_sz) {
    SYSTEMTIME st_utc, st_local;
    FileTimeToSystemTime(&ft, &st_utc);
    SystemTimeToTzSpecificLocalTime(NULL, &st_utc, &st_local);
    swprintf(out, out_sz, L"%04d-%02d-%02d %02d:%02d",
             st_local.wYear, st_local.wMonth, st_local.wDay,
             st_local.wHour, st_local.wMinute);
}

// --- Console Configuration ---
void init_console(void) {
    g_app.h_out = GetStdHandle(STD_OUTPUT_HANDLE);
    g_app.h_in  = GetStdHandle(STD_INPUT_HANDLE);

    GetConsoleMode(g_app.h_out, &g_app.orig_out_mode);
    GetConsoleMode(g_app.h_in,  &g_app.orig_in_mode);

    // Enable ANSI escape sequences & disable line buffering
    DWORD out_mode = g_app.orig_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
    SetConsoleMode(g_app.h_out, out_mode);

    // Hide cursor & switch to alternate buffer
    wprintf(L"\x1b[?1049h\x1b[?25l");
    fflush(stdout);
}

void restore_console(void) {
    // Show cursor & restore buffer
    wprintf(L"\x1b[?25h\x1b[?1049l");
    fflush(stdout);
    SetConsoleMode(g_app.h_out, g_app.orig_out_mode);
    SetConsoleMode(g_app.h_in,  g_app.orig_in_mode);
}

void update_dimensions(void) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(g_app.h_out, &csbi)) {
        g_app.term_width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        g_app.term_height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        g_app.term_width = 80;
        g_app.term_height = 25;
    }
}

// --- Sorting ---
int compare_entries(const void *a, const void *b) {
    const FileEntry *fa = (const FileEntry *)a;
    const FileEntry *fb = (const FileEntry *)b;

    // Directories always float to the top
    if (fa->is_dir && !fb->is_dir) return -1;
    if (!fa->is_dir && fb->is_dir) return 1;

    int res = 0;
    switch (g_app.sort_mode) {
        case SORT_SIZE:
            if (fa->size < fb->size) res = -1;
            else if (fa->size > fb->size) res = 1;
            break;
        case SORT_DATE:
            res = CompareFileTime(&fa->mtime, &fb->mtime);
            break;
        case SORT_NAME:
        default:
            res = _wcsicmp(fa->name, fb->name);
            break;
    }

    return g_app.reverse_sort ? -res : res;
}

void sort_entries(void) {
    if (g_app.count > 0) {
        qsort(g_app.entries, g_app.count, sizeof(FileEntry), compare_entries);
    }
}

// --- Filesystem Operations ---
void load_directory(void) {
    g_app.count = 0;
    g_app.selected = 0;
    g_app.scroll_offset = 0;

    wchar_t search_pattern[MAX_PATH];
    swprintf(search_pattern, MAX_PATH, L"%ls\\*", g_app.current_path);

    WIN32_FIND_DATAW fd;
    HANDLE h_find = FindFirstFileW(search_pattern, &fd);

    if (h_find == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        // Skip "."
        if (wcscmp(fd.cFileName, L".") == 0) continue;

        bool is_hidden = (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) || 
                         (fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM);

        if (is_hidden && !g_app.show_hidden) continue;

        if (g_app.count < MAX_ENTRIES) {
            FileEntry *entry = &g_app.entries[g_app.count++];
            wcsncpy(entry->name, fd.cFileName, MAX_PATH - 1);
            entry->name[MAX_PATH - 1] = L'\0';
            entry->is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            entry->is_hidden = is_hidden;
            entry->size = ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            entry->mtime = fd.ftLastWriteTime;
        }
    } while (FindNextFileW(h_find, &fd));

    FindClose(h_find);
    sort_entries();
}

// --- Help Modal Dialog ---
void render_help_modal(void) {
    const wchar_t *help_lines[] = {
        L"fsctl - File System Control (Help)",
        L"--------------------------------------------------",
        L"Navigation:",
        L"  Up / k       : Move cursor up",
        L"  Down / j     : Move cursor down",
        L"  Enter / l    : Open directory / launch file",
        L"  Backspace/h  : Navigate to parent directory",
        L"  Home / End   : Jump to top / bottom",
        L"",
        L"Display & Sorting:",
        L"  a            : Toggle hidden/system files",
        L"  s            : Cycle sort field (Name, Size, Date)",
        L"  r            : Toggle reverse sort order",
        L"",
        L"Actions:",
        L"  d            : Delete selected file (disabled in RO)",
        L"  ? or F1      : Toggle this help overlay",
        L"  q or Esc     : Close help / Quit program",
        L"--------------------------------------------------",
        L"Press any navigation key or ? to dismiss."
    };
    int total_lines = sizeof(help_lines) / sizeof(help_lines[0]);
    int box_width = 54;
    int start_y = (g_app.term_height - total_lines) / 2;
    int start_x = (g_app.term_width - box_width) / 2;

    for (int i = 0; i < total_lines; i++) {
        wprintf(L"\x1b[%d;%dH\x1b[44;37m  %-*ls\x1b[0m", 
                start_y + i, start_x, box_width - 2, help_lines[i]);
    }
}

// --- TUI Rendering Engine ---
void render_ui(void) {
    update_dimensions();

    // Clear Screen & Move to Top
    wprintf(L"\x1b[H");

    // Header Bar
    wprintf(L"\x1b[47;30m fsctl v%hs \x1b[44;37m Path: %-.*ls\x1b[K\x1b[0m\n",
            FSCTL_VERSION,
            g_app.term_width - 22,
            g_app.current_path);

    // Column Headers
    wprintf(L"\x1b[1;30;47m %-3s %-40.40ls %12s %20s \x1b[K\x1b[0m\n",
            L"TYP", L"NAME", L"SIZE", L"MODIFIED");

    int available_rows = g_app.term_height - 3; // header (2) + status (1)
    if (available_rows < 1) available_rows = 1;

    // Adjust scroll view
    if (g_app.selected < g_app.scroll_offset) {
        g_app.scroll_offset = g_app.selected;
    }
    if (g_app.selected >= g_app.scroll_offset + available_rows) {
        g_app.scroll_offset = g_app.selected - available_rows + 1;
    }

    // Render items
    for (int i = 0; i < available_rows; i++) {
        int idx = g_app.scroll_offset + i;
        wprintf(L"\x1b[%d;1H\x1b[K", i + 3);

        if (idx < g_app.count) {
            FileEntry *e = &g_app.entries[idx];
            bool is_curr = (idx == g_app.selected);

            wchar_t sz_buf[32];
            wchar_t dt_buf[32];
            format_filetime(e->mtime, dt_buf, 32);

            if (e->is_dir) {
                wcscpy(sz_buf, L"<DIR>");
            } else {
                format_size(e->size, sz_buf, 32);
            }

            // Selection styling
            if (is_curr) {
                wprintf(L"\x1b[7m"); // Reverse video
            }

            // Foreground color based on type
            if (e->is_dir) {
                wprintf(L"\x1b[36m"); // Cyan
            } else if (e->is_hidden) {
                wprintf(L"\x1b[90m"); // Muted Gray
            } else {
                wprintf(L"\x1b[37m"); // White
            }

            wprintf(L" %-3ls %-40.40ls %12ls %20ls ",
                    e->is_dir ? L"[D]" : L"   ",
                    e->name,
                    sz_buf,
                    dt_buf);

            wprintf(L"\x1b[0m");
        }
    }

    // Status / Bottom Bar
    const wchar_t *sort_names[] = { L"Name", L"Size", L"Date" };
    wprintf(L"\x1b[%d;1H\x1b[47;30m [%d/%d] | Sort: %ls (%ls) | %ls | ?:Help | q:Quit\x1b[K\x1b[0m",
            g_app.term_height,
            g_app.count > 0 ? g_app.selected + 1 : 0,
            g_app.count,
            sort_names[g_app.sort_mode],
            g_app.reverse_sort ? L"DESC" : L"ASC",
            g_app.readonly_mode ? L"READ-ONLY" : L"READ/WRITE");

    if (g_app.show_help) {
        render_help_modal();
    }

    fflush(stdout);
}

// --- Input Handling ---
void handle_input(void) {
    int ch = _getch();

    if (ch == 0 || ch == 224) {
        // Extended Key Sequences
        int ext = _getch();
        if (g_app.show_help) {
            g_app.show_help = false;
            return;
        }

        switch (ext) {
            case 72: // UP
                if (g_app.selected > 0) g_app.selected--;
                break;
            case 80: // DOWN
                if (g_app.selected < g_app.count - 1) g_app.selected++;
                break;
            case 71: // HOME
                g_app.selected = 0;
                break;
            case 79: // END
                if (g_app.count > 0) g_app.selected = g_app.count - 1;
                break;
            case 59: // F1
                g_app.show_help = !g_app.show_help;
                break;
        }
        return;
    }

    if (g_app.show_help) {
        g_app.show_help = false;
        return;
    }

    switch (ch) {
        case 'q':
        case 27: // ESC
            restore_console();
            exit(0);
            break;

        case '?':
            g_app.show_help = true;
            break;

        case 'k':
            if (g_app.selected > 0) g_app.selected--;
            break;

        case 'j':
            if (g_app.selected < g_app.count - 1) g_app.selected++;
            break;

        case 'a': // Toggle Hidden
            g_app.show_hidden = !g_app.show_hidden;
            load_directory();
            break;

        case 's': // Cycle Sort
            g_app.sort_mode = (g_app.sort_mode + 1) % 3;
            sort_entries();
            break;

        case 'r': // Invert Sort Order
            g_app.reverse_sort = !g_app.reverse_sort;
            sort_entries();
            break;

        case 8:   // Backspace (Go to parent)
        case 'h': {
            wchar_t *last_slash = wcsrchr(g_app.current_path, L'\\');
            if (last_slash && last_slash != g_app.current_path + 2) {
                *last_slash = L'\0';
                load_directory();
            } else if (last_slash && last_slash == g_app.current_path + 2) {
                // At drive root (e.g. C:\)
                *(last_slash + 1) = L'\0';
                load_directory();
            }
            break;
        }

        case 13:  // Enter
        case 'l': {
            if (g_app.count == 0) break;
            FileEntry *curr = &g_app.entries[g_app.selected];
            if (curr->is_dir) {
                if (wcscmp(curr->name, L"..") == 0) {
                    wchar_t *last_slash = wcsrchr(g_app.current_path, L'\\');
                    if (last_slash) *last_slash = L'\0';
                } else {
                    size_t len = wcslen(g_app.current_path);
                    if (g_app.current_path[len - 1] != L'\\') {
                        wcscat(g_app.current_path, L"\\");
                    }
                    wcscat(g_app.current_path, curr->name);
                }
                load_directory();
            } else {
                // ShellExecute to launch the file with its default Windows association
                wchar_t target[MAX_PATH];
                swprintf(target, MAX_PATH, L"%ls\\%ls", g_app.current_path, curr->name);
                ShellExecuteW(NULL, L"open", target, NULL, NULL, SW_SHOWNORMAL);
            }
            break;
        }

        case 'd': {
            if (g_app.readonly_mode || g_app.count == 0) break;
            FileEntry *curr = &g_app.entries[g_app.selected];
            wchar_t target[MAX_PATH];
            swprintf(target, MAX_PATH, L"%ls\\%ls", g_app.current_path, curr->name);
            
            bool success = curr->is_dir ? RemoveDirectoryW(target) : DeleteFileW(target);
            if (success) {
                load_directory();
            }
            break;
        }
    }
}

// --- CLI Argument Parsing & Help ---
void print_cli_help(const wchar_t *prog_name) {
    wprintf(
        L"fsctl - File System Control for Windows (v%hs)\n\n"
        L"USAGE:\n"
        L"  %ls [QUALIFIERS]\n\n"
        L"QUALIFIERS:\n"
        L"  -p, --path <DIR>          Specify starting directory [Default: .]\n"
        L"  -a, --all                 Display hidden and system files\n"
        L"  -s, --sort <MODE>         Sort mode: 'name', 'size', or 'date' [Default: name]\n"
        L"  -r, --reverse             Reverse the listing order\n"
        L"  -R, --readonly            Enable read-only mode (prevent file deletion)\n"
        L"  -h, --help                Show this command line help and exit\n"
        L"  -v, --version             Display version information\n\n"
        L"INTERACTIVE KEYBINDINGS:\n"
        L"  [Up/Down] or [k/j]        Navigate selection\n"
        L"  [Enter]   or [l]          Open directory or file\n"
        L"  [Backspace] or [h]        Navigate to parent directory\n"
        L"  [a]                       Toggle hidden/system files\n"
        L"  [s]                       Cycle sort order\n"
        L"  [r]                       Toggle sort order direction\n"
        L"  [d]                       Delete selected item (Disabled in Read-Only)\n"
        L"  [?] or [F1]               Show help modal\n"
        L"  [q] or [ESC]              Exit fsctl\n",
        FSCTL_VERSION, prog_name
    );
}

int wmain(int argc, wchar_t *argv[]) {
    // Defaults
    GetCurrentDirectoryW(MAX_PATH, g_app.current_path);
    g_app.show_hidden = false;
    g_app.sort_mode = SORT_NAME;
    g_app.reverse_sort = false;
    g_app.readonly_mode = false;
    g_app.show_help = false;

    // CLI Qualifiers Processing
    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"-h") == 0 || wcscmp(argv[i], L"--help") == 0) {
            print_cli_help(argv[0]);
            return 0;
        } else if (wcscmp(argv[i], L"-v") == 0 || wcscmp(argv[i], L"--version") == 0) {
            wprintf(L"fsctl version %hs\n", FSCTL_VERSION);
            return 0;
        } else if (wcscmp(argv[i], L"-a") == 0 || wcscmp(argv[i], L"--all") == 0) {
            g_app.show_hidden = true;
        } else if (wcscmp(argv[i], L"-r") == 0 || wcscmp(argv[i], L"--reverse") == 0) {
            g_app.reverse_sort = true;
        } else if (wcscmp(argv[i], L"-R") == 0 || wcscmp(argv[i], L"--readonly") == 0) {
            g_app.readonly_mode = true;
        } else if ((wcscmp(argv[i], L"-p") == 0 || wcscmp(argv[i], L"--path") == 0) && i + 1 < argc) {
            GetFullPathNameW(argv[++i], MAX_PATH, g_app.current_path, NULL);
        } else if ((wcscmp(argv[i], L"-s") == 0 || wcscmp(argv[i], L"--sort") == 0) && i + 1 < argc) {
            i++;
            if (_wcsicmp(argv[i], L"size") == 0) g_app.sort_mode = SORT_SIZE;
            else if (_wcsicmp(argv[i], L"date") == 0) g_app.sort_mode = SORT_DATE;
            else g_app.sort_mode = SORT_NAME;
        } else {
            wprintf(L"Unknown qualifier: %ls\nTry '%ls --help' for options.\n", argv[i], argv[0]);
            return 1;
        }
    }

    init_console();
    load_directory();

    while (1) {
        render_ui();
        handle_input();
    }

    restore_console();
    return 0;
}