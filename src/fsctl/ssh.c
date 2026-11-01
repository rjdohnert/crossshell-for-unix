#include "app.h"
#include "ssh.h"

// --- SSH Support ---
void ssh_connect(void) {
    wchar_t args[MAX_PATH * 2];
    swprintf(args, MAX_PATH * 2, L"/k cd /d \"%ls\" & ssh", g_app.current_path);
    ShellExecuteW(NULL, L"open", L"cmd.exe", args, NULL, SW_SHOWNORMAL);
}
