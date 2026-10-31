#include "common.h"
#include "app.h"
#include "cleanup.h"

int wmain(int argc, wchar_t *argv[]) {
    srand((unsigned int)GetTickCount64() ^ GetCurrentProcessId());
    cleanup_init();

    CompressApp *app = CompressApp_create();
    if (!app) {
        fwprintf(stderr, L"Fatal: out of memory initializing runtime.\n");
        cleanup_shutdown();
        return 1;
    }

    int result = CompressApp_run(app, argc, argv);
    CompressApp_destroy(app);

    cleanup_shutdown();
    return result;
}
