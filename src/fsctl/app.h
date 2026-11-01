#ifndef FSCTL_APP_H
#define FSCTL_APP_H

#define UNICODE
#define _UNICODE
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <shellapi.h>
#include <winnetwk.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <wchar.h>
#include <conio.h>
#include <time.h>

#define FSCTL_VERSION "6.5.0"
#define INITIAL_CAPACITY 512

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
    FileEntry *entries;
    int count;
    int capacity;
    int selected;
    int scroll_offset;

    // Qualifiers / Settings
    bool show_hidden;
    SortMode sort_mode;
    bool reverse_sort;
    bool readonly_mode;
    bool show_help;

    // Filter & Clipboard
    wchar_t filter[MAX_PATH];
    wchar_t clipboard_path[MAX_PATH];
    bool clipboard_is_cut;

    // Console dimensions
    int term_width;
    int term_height;

    // Win32 Console State
    HANDLE h_out;
    HANDLE h_in;
    DWORD orig_out_mode;
    DWORD orig_in_mode;
    bool console_initialized;
} AppState;

extern AppState g_app;

#endif // FSCTL_APP_H
