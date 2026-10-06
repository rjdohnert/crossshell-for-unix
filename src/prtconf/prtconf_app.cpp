#include "prtconf_app.hpp"

int PrtconfApp::run(int argc, wchar_t* argv[]) {
    PrtconfOptions options;
    if (!PrtconfOptions::parse(argc, argv, options)) {
        std::cerr << "Try 'prtconf --help' for usage.\n";
        return 2;
    }
    PrtconfEngine engine(options);
    return engine.execute();
}
