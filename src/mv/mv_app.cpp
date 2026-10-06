#include "mv_app.hpp"

int MoveApp::run(int argc, char* argv[]) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }

    MoveOptions options;
    if (!ArgumentParser::parse(argc, argv, options)) {
        return 1;
    }

    if (options.showHelp) {
        HelpFormatter::printHelp();
        return 0;
    }

    if (options.showVersion) {
        HelpFormatter::printVersion();
        return 0;
    }

    MoveEngine engine(options);
    return engine.execute() ? 0 : 1;
}
