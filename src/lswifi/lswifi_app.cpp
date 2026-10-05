#include "lswifi_app.hpp"
#include "options.hpp"
#include "engine.hpp"
#include "reporter.hpp"
#include <iostream>
#include <memory>

int LswifiApplication::Run(int argc, char* argv[]) {
    RuntimeConfig cfg;
    if (!cfg.Parse(argc, argv)) {
        return 0; // Help or version was handled
    }

    // Enable Windows ANSI escape codes if writing to a real console
    if (cfg.useColor) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }

    try {
        WifiManager mgr;
        auto adapters = mgr.EnumerateAdapters(cfg.triggerScan);

        std::unique_ptr<IFormatter> formatter = FormatterFactory::Create(cfg.format);
        formatter->Output(adapters, cfg);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
