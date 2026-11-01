#include "common.h"
#include "app.h"

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    char** u8argv = (char**)calloc(argc, sizeof(char*));
    if (!u8argv) {
        fprintf(stderr, "bdf: allocation failed\n");
        return 1;
    }

    for (int i = 0; i < argc; ++i) {
        u8argv[i] = Utf16ToUtf8(argv[i]);
        if (!u8argv[i]) {
            u8argv[i] = SafeStrDup("");
        }
    }

    BdfApplication* app = BdfApplication_Create();
    int exit_code = 1;
    if (app) {
        exit_code = BdfApplication_Run(app, argc, u8argv);
        BdfApplication_Destroy(app);
    } else {
        fprintf(stderr, "bdf: allocation failed\n");
    }

    for (int i = 0; i < argc; ++i) {
        free(u8argv[i]);
    }
    free(u8argv);

    return exit_code;
}
