/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#include "logger_app.hpp"
#include "options.hpp"
#include "engine.hpp"
#include <iostream>

int LoggerApplication::Run(int argc, wchar_t* argv[]) {
    LoggerOptions opts;
    if (!opts.Parse(argc, argv)) {
        opts.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"logger");
        return 1;
    }

    if (opts.showHelp) {
        opts.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"logger");
        return 0;
    }

    if (opts.showVersion) {
        opts.PrintVersion();
        return 0;
    }

    if (!opts.messageParts.empty()) {
        EventLogEngine::LogLine(opts.tag, LoggerOptions::JoinWords(opts.messageParts, 0));
        return 0;
    }

    std::wstring line;
    while (std::getline(std::wcin, line)) {
        EventLogEngine::LogLine(opts.tag, line);
    }

    return 0;
}
