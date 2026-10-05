class FalseOptions {
public:
    int outputFormat{0}; // 0: none, 1: JSON, 2: CSV, 3: Table
    std::wstring pipeCommand;

    static void printHelp() {
        std::wcout << LR"(false(1)            CrossShell for UNIX Reference Manual                 false(1)

    NAME
        false - do nothing, unsuccessfully

    SYNOPSIS
        false [OPTIONS] [ARGUMENTS...]

    DESCRIPTION
        Exit with a status code indicating failure (1). Ignores any command-line
        arguments provided, returning an exit code of 1 immediately.

    OPTIONS
        --json
            Output failure status formatted as JSON.

        --csv
            Output failure status formatted as CSV.

        --table
            Output failure status formatted as a table.

        --pipe COMMAND
            Send status output through the specified pipe command.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        false
            Execute false and return exit code 1.

        false || echo "Command failed"
            Conditional execution upon failure.

    CrossShell for UNIX                                                    false(1)
)";
    }

    static void printVersion() {
        std::wcout << L"false 1.0.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], FalseOptions& opts) {
        if (argc > 1) {
            std::wstring arg = argv[1] ? argv[1] : L"";
            if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
                printHelp();
                std::exit(0);
            }
            if (arg == L"--version" || arg == L"-V") {
                printVersion();
                std::exit(0);
            }
        }

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--json") opts.outputFormat = 1;
            else if (arg == L"--csv") opts.outputFormat = 2;
            else if (arg == L"--table") opts.outputFormat = 3;
            else if (arg == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            }
        }
        return true;
    }
};
