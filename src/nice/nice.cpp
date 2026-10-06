#include "nice_app.hpp"

int wmain(int argc, wchar_t* argv[]) {
    NiceApplication app;
    return app.Run(argc, argv);
}
