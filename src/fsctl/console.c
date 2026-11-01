#include "app.h"
#include "console.h"

// --- Console Ctrl Handler ---
static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    (void)dwCtrlType;
    restore_console();
    return FALSE;
}

// --- Console Configuration ---
void init_console(void) {
    g_app.h_out = GetStdHandle(STD_OUTPUT_HANDLE);
    g_app.h_in  = GetStdHandle(STD_INPUT_HANDLE);

    GetConsoleMode(g_app.h_out, &g_app.orig_out_mode);
    GetConsoleMode(g_app.h_in,  &g_app.orig_in_mode);

    DWORD out_mode = g_app.orig_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
    SetConsoleMode(g_app.h_out, out_mode);

    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    g_app.console_initialized = true;

    wprintf(L"\x1b[?1049h\x1b[?25l");
    fflush(stdout);
}

void restore_console(void) {
    if (!g_app.console_initialized) return;
    g_app.console_initialized = false;
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
