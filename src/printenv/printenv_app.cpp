#include "printenv_app.hpp"

int PrintenvApp::run(int argc, wchar_t* argv[]) {
    PrintenvOptions options;
    if (!PrintenvOptions::parse(argc, argv, options)) {
        return 1;
    }
    return PrintenvEngine::execute(options);
}
