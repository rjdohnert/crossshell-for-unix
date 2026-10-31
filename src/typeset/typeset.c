#include "common.h"
#include "app.h"

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    TypesetApp* app = TypesetApp_Create();
    if (!app) {
        fprintf(stderr, "typeset: failed to allocate application runtime\n");
        return 1;
    }

    int ret = TypesetApp_Run(app, argc, argv);
    TypesetApp_Destroy(app);
    return ret;
}
