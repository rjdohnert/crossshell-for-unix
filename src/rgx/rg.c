#include "command_line.h"
#include "allocation.h"

int wmain(int argc, wchar_t** wide_argv) {
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    DWORD console_mode;
    if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &console_mode)) SetConsoleOutputCP(CP_UTF8);
    char** argv = (char**)rgx_calloc((size_t)argc + 1, sizeof(char*));
    int status = 2;
    for (int index = 0; index < argc; ++index) {
        int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_argv[index], -1, NULL, 0, NULL, NULL);
        if (!required) {
            fprintf(stderr, "rgx: invalid Unicode command-line argument\n");
            goto cleanup_arguments;
        }
        argv[index] = (char*)rgx_malloc((size_t)required);
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_argv[index], -1, argv[index], required, NULL, NULL)) goto cleanup_arguments;
    }
    status = rgx_main(argc, argv);
    if (fflush(stdout) == EOF || ferror(stdout)) {
        fprintf(stderr, "rgx: output write failed\n");
        status = 2;
    }
cleanup_arguments:
    for (int index = 0; index < argc; ++index) free(argv[index]);
    free(argv);
    return status;
}
