#include "nfsctl_app.hpp"

int wmain(int argc, wchar_t* argv[]) {
    NfsctlApplication app;
    return app.Run(argc, argv);
}
