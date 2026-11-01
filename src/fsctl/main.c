#include "app.h"
#include "cli.h"
#include "console.h"
#include "dirlist.h"
#include "input.h"
#include "render.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "mpr.lib")

AppState g_app;

static void app_init_defaults(void) {
    GetCurrentDirectoryW(MAX_PATH, g_app.current_path);
    g_app.entries = NULL;
    g_app.count = 0;
    g_app.capacity = 0;
    g_app.show_hidden = false;
    g_app.sort_mode = SORT_NAME;
    g_app.reverse_sort = false;
    g_app.readonly_mode = false;
    g_app.show_help = false;
    g_app.console_initialized = false;
    g_app.filter[0] = L'\0';
    g_app.clipboard_path[0] = L'\0';
    g_app.clipboard_is_cut = false;
}

int wmain(int argc, wchar_t *argv[]) {
    app_init_defaults();

    int cli_result = cli_parse_args(argc, argv);
    if (cli_result >= 0) {
        return cli_result;
    }

    init_console();
    load_directory();

    while (1) {
        render_ui();
        handle_input();
    }

    restore_console();
    if (g_app.entries) free(g_app.entries);
    return 0;
}
