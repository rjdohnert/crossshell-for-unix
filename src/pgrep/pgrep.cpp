#include "pgrep_app.hpp"

int wmain(int argc, wchar_t* argv[]) {
    PgrepApplication app;
    return app.Run(argc, argv);
}
