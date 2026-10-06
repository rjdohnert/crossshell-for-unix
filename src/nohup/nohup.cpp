#include "nohup_app.hpp"

int wmain(int argc, wchar_t* argv[]) {
    NohupApplication app;
    return app.Run(argc, argv);
}