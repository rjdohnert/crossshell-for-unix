#include "app.h"
#include "render.h"
#include "console.h"
#include "format.h"

// --- Help Modal Dialog ---
static void render_help_modal(void) {
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
        L"Filter & Clipboard:",
        L"  /            : Interactive search/filter (Esc clears)",
        L"  y            : Copy (Yank) selected file or folder",
        L"  x            : Cut selected file or folder",
        L"  p            : Paste copied/cut item into current dir",
        L"",
        L"Display & Sorting:",
        L"  a            : Toggle hidden/system files",
        L"  s            : Cycle sort field (Name, Size, Date)",
        L"  r            : Toggle reverse sort order",
        L"",
        L"Actions:",
        L"  d            : Safe delete to Recycle Bin (Prompted)",
        L"  R            : Rename selected file/directory",
        L"  n            : List mapped network drives",
        L"  v            : Switch drive (volume selector)",
        L"  S            : Launch SSH in new terminal",
        L"  ? or F1      : Toggle this help overlay",
        L"  q or Esc     : Close help / Quit program",
        L"--------------------------------------------------",
        L"Press any navigation key or ? to dismiss."
    };
    int total_lines = sizeof(help_lines) / sizeof(help_lines[0]);
    int box_width = 62;
    int start_y = (g_app.term_height - total_lines) / 2;
    int start_x = (g_app.term_width - box_width) / 2;
    if (start_x < 1) start_x = 1;
    if (start_y < 1) start_y = 1;

    for (int i = 0; i < total_lines; i++) {
        wprintf(L"\x1b[%d;%dH\x1b[44;37m  %-*ls\x1b[0m",
                start_y + i, start_x, box_width - 2, help_lines[i]);
    }
}

// --- TUI Rendering Engine ---
static wchar_t g_framebuf[65536];

void render_ui(void) {
    update_dimensions();

    wchar_t *p = g_framebuf;
    wchar_t *end = g_framebuf + 65536 - 256;

    p += swprintf(p, end - p, L"\x1b[H");

    // Header Bar
    {
        SYSTEMTIME st;
        GetLocalTime(&st);

        p += swprintf(p, end - p, L"\x1b[47;30m fsctl v%hs \x1b[44;37m %02d/%02d/%04d  Path: %-.*ls\x1b[K\x1b[0m\n",
                FSCTL_VERSION,
                st.wMonth, st.wDay, st.wYear,
                g_app.term_width - 40,
                g_app.current_path);
    }

    // Column Headers
    p += swprintf(p, end - p, L"\x1b[1;30;47m\x1b[K");
    p += swprintf(p, end - p, L"\x1b[2;3H%-40.40ls", L"NAME");
    p += swprintf(p, end - p, L"\x1b[2;44H%-6ls", L"TYPE");
    p += swprintf(p, end - p, L"\x1b[2;51H%-14ls", L"SIZE");
    p += swprintf(p, end - p, L"\x1b[2;66H%-22ls", L"MODIFIED");
    p += swprintf(p, end - p, L"\x1b[0m\n");

    int available_rows = g_app.term_height - 3;
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
        p += swprintf(p, end - p, L"\x1b[%d;1H\x1b[K", i + 3);

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

            if (is_curr) p += swprintf(p, end - p, L"\x1b[7m");

            if (e->is_dir) {
                p += swprintf(p, end - p, L"\x1b[36m");
            } else if (e->is_hidden) {
                p += swprintf(p, end - p, L"\x1b[90m");
            } else {
                p += swprintf(p, end - p, L"\x1b[37m");
            }

            int row = i + 3;
            p += swprintf(p, end - p, L"\x1b[%d;3H%-40.40ls", row, e->name);
            p += swprintf(p, end - p, L"\x1b[%d;44H%-6ls", row, e->is_dir ? L"[DIR]" : L"");
            p += swprintf(p, end - p, L"\x1b[%d;51H%-14ls", row, sz_buf);
            p += swprintf(p, end - p, L"\x1b[%d;66H%-22ls", row, dt_buf);

            p += swprintf(p, end - p, L"\x1b[0m");
        }
    }

    // Status / Bottom Bar
    const wchar_t *sort_names[] = { L"Name", L"Size", L"Date" };
    wchar_t extra_info[128] = L"";
    if (g_app.filter[0] != L'\0') {
        swprintf(extra_info, 128, L" | Filter: '%ls'", g_app.filter);
    } else if (g_app.clipboard_path[0] != L'\0') {
        const wchar_t *slash = wcsrchr(g_app.clipboard_path, L'\\');
        const wchar_t *clip_name = slash ? slash + 1 : g_app.clipboard_path;
        swprintf(extra_info, 128, L" | %ls: '%ls'", g_app.clipboard_is_cut ? L"Cut" : L"Copied", clip_name);
    }

    p += swprintf(p, end - p, L"\x1b[%d;1H\x1b[47;30m [%d/%d] | Sort: %ls (%ls)%ls | /:Search | ?:Help\x1b[K\x1b[0m",
            g_app.term_height,
            g_app.count > 0 ? g_app.selected + 1 : 0,
            g_app.count,
            sort_names[g_app.sort_mode],
            g_app.reverse_sort ? L"DESC" : L"ASC",
            extra_info);

    DWORD written;
    WriteConsoleW(g_app.h_out, g_framebuf, (DWORD)(p - g_framebuf), &written, NULL);

    if (g_app.show_help) {
        render_help_modal();
        fflush(stdout);
    }
}
