#include "strace_app.hpp"

int wmain(int argc, wchar_t* argv[]) {
    StraceApplication app;
    return app.Run(argc, argv);
}
