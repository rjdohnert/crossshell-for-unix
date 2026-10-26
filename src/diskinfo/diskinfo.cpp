#include "diskinfo_app.hpp"
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#include <windows.h>

int wmain(int argc, wchar_t* argv[]) {
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);
    SetConsoleOutputCP(CP_UTF8);
    DiskinfoApplication app;
    return app.Run(argc, argv);
}
