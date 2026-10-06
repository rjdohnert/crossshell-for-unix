#include "pkg_app.hpp"

int PkgApp::run(int argc, wchar_t* argv[]) {
    PkgOptions options;
    bool showHelp = false;
    bool showVersion = false;

    if (!PkgOptions::parse(argc, argv, options, showHelp, showVersion)) {
        PkgOptions::printHelp();
        return 1;
    }

    if (showHelp) {
        PkgOptions::printHelp();
        return 0;
    }

    if (showVersion) {
        PkgOptions::printVersion();
        return 0;
    }

    return WingetBridge::execute(options);
}
