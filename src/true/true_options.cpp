#include "true_options.hpp"

void TrueOptions::printHelp() {
        std::wcout << LR"(true(1)                 CrossShell for UNIX Reference Manual                 true(1)

    NAME
        true - return successful exit status

    SYNOPSIS
        true [OPTIONS] [ARGUMENTS...]

    DESCRIPTION
        Exit with a status code indicating success.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        --json
            Output success status as JSON format.

        --csv
            Output success status as CSV format.

        --table
            Output success status as formatted table.

        --pipe COMMAND
            Send status through pipeline COMMAND.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        true
            Execute and return exit status code 0.

    CrossShell for UNIX                                                      true(1)
)";
    }

void TrueOptions::printVersion() {
        std::wcout << L"true 1.0.0\n";
    }

bool TrueOptions::parse(int argc, wchar_t* argv[], TrueOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--help" || arg == L"-h" || arg == L"/?" || arg == L"-?") {
                printHelp();
                std::exit(0);
            }
            if (arg == L"--version" || arg == L"-V" || arg == L"-v") {
                printVersion();
                std::exit(0);
            }
            if (arg == L"--json") opts.outputFormat = 1;
            else if (arg == L"--csv") opts.outputFormat = 2;
            else if (arg == L"--table") opts.outputFormat = 3;
            else if (arg == L"--pipe") {
                if (i + 1 >= argc) return false;
                opts.pipeCommand = argv[++i];
            }
        }
        return true;
    }
