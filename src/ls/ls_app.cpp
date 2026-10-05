#include "ls_app.hpp"
#include "engine.hpp"
#include <iostream>

LsApp::LsApp(ListingOptions opts) : options(std::move(opts)) {}

int LsApp::run() {
    // Enable ANSI TrueColor / Virtual Terminal Sequences in Windows Console
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }

    ListingEngine engine(options);
    for (size_t i = 0; i < options.targets.size(); ++i) {
        fs::path p(options.targets[i]);
        if (!fs::exists(p)) {
            std::cerr << "ls: " << options.targets[i] << " not found\n";
            continue;
        }

        bool printHeader = options.targets.size() > 1;
        if (fs::is_directory(p) && options.outputFormat.empty()) {
            engine.listDirectory(p, printHeader);
        } else if (!fs::is_directory(p) && options.outputFormat.empty()) {
            engine.listSingleItem(p);
        } else {
            engine.listStructuredTarget(p);
        }
    }

    return 0;
}
