#include "pskill.hpp"
#include "pskill_app.hpp"

int wmain(int argc, wchar_t* argv[]) {
    PskillApplication app;
    return app.Run(argc, argv);
}