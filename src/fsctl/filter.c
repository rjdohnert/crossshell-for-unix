#include "app.h"
#include "filter.h"
#include "dirlist.h"

// --- Interactive Search / Filter Bar ---
void prompt_search_filter(void) {
    wprintf(L"\x1b[%d;1H\x1b[K\x1b[43;30m Search / Filter (Esc to clear): %ls\x1b[0m",
            g_app.term_height, g_app.filter);
    fflush(stdout);

    int prompt_len = 33;
    wchar_t buf[MAX_PATH];
    wcsncpy(buf, g_app.filter, MAX_PATH - 1);
    buf[MAX_PATH - 1] = L'\0';
    int pos = (int)wcslen(buf);

    while (true) {
        int c = _getch();
        if (c == 27) { // Esc — clear filter
            g_app.filter[0] = L'\0';
            load_directory();
            return;
        }
        if (c == 13) { // Enter — commit filter
            wcsncpy(g_app.filter, buf, MAX_PATH - 1);
            g_app.filter[MAX_PATH - 1] = L'\0';
            load_directory();
            return;
        }
        if (c == 8) { // Backspace
            if (pos > 0) {
                pos--;
                buf[pos] = L'\0';
                wprintf(L"\x1b[%d;%dH \x1b[%d;%dH", g_app.term_height, prompt_len + pos + 1,
                        g_app.term_height, prompt_len + pos + 1);
                fflush(stdout);
            }
            continue;
        }
        if (c >= 32 && pos < MAX_PATH - 1) {
            buf[pos++] = (wchar_t)c;
            buf[pos] = L'\0';
            wprintf(L"%lc", (wchar_t)c);
            fflush(stdout);
        }
    }
}
