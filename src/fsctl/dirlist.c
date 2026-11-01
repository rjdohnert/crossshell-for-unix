#include "app.h"
#include "dirlist.h"

// --- Case-insensitive Substring Search Helper ---
static bool contains_ci(const wchar_t *haystack, const wchar_t *needle) {
    if (!needle || !*needle) return true;
    wchar_t h[MAX_PATH], n[MAX_PATH];
    wcsncpy(h, haystack, MAX_PATH - 1); h[MAX_PATH - 1] = L'\0';
    wcsncpy(n, needle, MAX_PATH - 1); n[MAX_PATH - 1] = L'\0';
    _wcslwr(h);
    _wcslwr(n);
    return wcsstr(h, n) != NULL;
}

// --- Sorting ---
static int compare_entries(const void *a, const void *b) {
    const FileEntry *fa = (const FileEntry *)a;
    const FileEntry *fb = (const FileEntry *)b;

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
    if (g_app.count > 0 && g_app.entries) {
        qsort(g_app.entries, g_app.count, sizeof(FileEntry), compare_entries);
    }
}

// --- Filesystem Operations ---
static void ensure_entries_capacity(int min_capacity) {
    if (min_capacity <= g_app.capacity) return;
    int new_cap = (g_app.capacity == 0) ? INITIAL_CAPACITY : g_app.capacity * 2;
    while (new_cap < min_capacity) new_cap *= 2;
    FileEntry *new_entries = (FileEntry*)realloc(g_app.entries, sizeof(FileEntry) * new_cap);
    if (new_entries) {
        g_app.entries = new_entries;
        g_app.capacity = new_cap;
    }
}

void load_directory(void) {
    g_app.count = 0;
    g_app.selected = 0;
    g_app.scroll_offset = 0;

    wchar_t search_pattern[MAX_PATH];
    if (swprintf_s(search_pattern, MAX_PATH, L"%ls\\*", g_app.current_path) < 0) {
        return;
    }

    WIN32_FIND_DATAW fd;
    HANDLE h_find = FindFirstFileW(search_pattern, &fd);

    if (h_find == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (wcscmp(fd.cFileName, L".") == 0) continue;

        bool is_hidden = (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ||
                         (fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM);

        if (is_hidden && !g_app.show_hidden) continue;

        // Apply interactive search/filter if present
        if (g_app.filter[0] != L'\0' && !contains_ci(fd.cFileName, g_app.filter)) {
            continue;
        }

        ensure_entries_capacity(g_app.count + 1);
        if (g_app.count < g_app.capacity) {
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
