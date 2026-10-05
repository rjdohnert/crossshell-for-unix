/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#include "options.hpp"
#include <iostream>

std::wstring LoggerOptions::JoinWords(const std::vector<std::wstring>& words, size_t startIndex) {
    std::wstring message;
    for (size_t i = startIndex; i < words.size(); ++i) {
        if (i > startIndex) {
            message.push_back(L' ');
        }
        message += words[i];
    }
    return message;
}

bool LoggerOptions::Parse(int argc, wchar_t* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i] ? argv[i] : L"";
        if (arg == L"--") {
            for (++i; i < argc; ++i) {
                messageParts.push_back(argv[i] ? argv[i] : L"");
            }
            break;
        }
        if (arg == L"-h" || arg == L"--help") {
            showHelp = true;
            return true;
        }
        if (arg == L"-V" || arg == L"--version") {
            showVersion = true;
            return true;
        }
        if (arg == L"-t" || arg == L"--tag") {
            if (i + 1 >= argc) {
                std::wcerr << L"logger: option requires an argument -- t\n";
                return false;
            }
            tag = argv[++i] ? argv[i] : L"logger";
            continue;
        }
        if (arg.rfind(L"--tag=", 0) == 0) {
            tag = arg.substr(6);
            continue;
        }
        if (!arg.empty() && arg[0] == L'-') {
            std::wcerr << L"logger: unknown option -- " << arg << L"\n";
            return false;
        }

        messageParts.push_back(arg);
    }
    return true;
}

void LoggerOptions::PrintUsage(const wchar_t* /*progName*/) const {
    std::wcout << LR"(logger(1)               CrossShell for UNIX Reference Manual                logger(1)

    NAME
        logger - enter messages into the Windows Event Log or syslog stream

    SYNOPSIS
        logger [OPTIONS] [MESSAGE...]

    DESCRIPTION
        logger makes entries in the Windows Application Event Log or syslog sink.
        When MESSAGE is not specified, logger reads from standard input.

    OPTIONS
        -t, --tag TAG
            Mark every line in the log with the specified TAG.

        -p, --priority PRI
            Enter the message with the specified priority (info, warn, err).

        -s, --stderr
            Output the message to standard error as well as the system log.

        -f, --file FILE
            Log the contents of the specified file.

        --json, --csv, --table
            Output logging execution status in structured format.

        --pipe COMMAND
            Stream results into COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        logger -t Backup "Backup completed successfully"
            Log informational event to Windows Event Log.

    CrossShell for UNIX                                                 logger(1)
)";
}

void LoggerOptions::PrintVersion() const {
    std::wcout << L"logger 1.0.0\n";
}
