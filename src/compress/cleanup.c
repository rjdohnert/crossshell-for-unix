#include "cleanup.h"

static wchar_t *g_active_temp_file = NULL;
static CRITICAL_SECTION g_cleanup_lock;

void register_active_temp_file(const wchar_t *path) {
    EnterCriticalSection(&g_cleanup_lock);
    if (g_active_temp_file) {
        free(g_active_temp_file);
        g_active_temp_file = NULL;
    }
    if (path) {
        g_active_temp_file = _wcsdup(path);
    }
    LeaveCriticalSection(&g_cleanup_lock);
}

static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    switch (ctrl_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            EnterCriticalSection(&g_cleanup_lock);
            if (g_active_temp_file) {
                DeleteFileW(g_active_temp_file);
                free(g_active_temp_file);
                g_active_temp_file = NULL;
            }
            LeaveCriticalSection(&g_cleanup_lock);
            break;
        default:
            break;
    }
    return FALSE;
}

void cleanup_init(void) {
    InitializeCriticalSection(&g_cleanup_lock);
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
}

void cleanup_shutdown(void) {
    DeleteCriticalSection(&g_cleanup_lock);
}
