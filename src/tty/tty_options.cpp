#include "tty_options.hpp"

void TtyOptions::printHelp() {
        std::wcout << LR"(tty(1)                  CrossShell for UNIX Reference Manual                 tty(1)

    NAME
        tty - print the file name of the terminal connected to standard input

    SYNOPSIS
        tty [OPTIONS]

    DESCRIPTION
        Print the file name of the terminal connected to standard input.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -s, -q, --silent, --quiet
            Print nothing, only return an exit status.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        tty
            Print name of current terminal device.

        tty -s
            Check if standard input is a terminal without producing output.

    CrossShell for UNIX                                                      tty(1)
)";
    }

void TtyOptions::printVersion() {
        std::wcout << L"tty 1.0\n";
    }

bool TtyOptions::parse(int argc, wchar_t* argv[], TtyOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"-s" || arg == L"-q" || arg == L"--silent" || arg == L"--quiet") {
                opts.silent = true;
            } else if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                printHelp();
                std::exit(0);
            } else if (arg == L"--version" || arg == L"-v") {
                printVersion();
                std::exit(0);
            } else {
                std::wcerr << L"tty: invalid option: " << arg << L"\n";
                std::wcerr << L"Try 'tty --help' for more information.\n";
                return false;
            }
        }
        return true;
    }
