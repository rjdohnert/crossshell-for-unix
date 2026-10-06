#include "option_parser.hpp"
#include "recycle_options.hpp"
#include "wide_pipe_buffer.hpp"

void OptionParser::PrintHelp() {
        std::wcout << LR"(recycle(1)                CrossShell for UNIX Reference Manual                recycle(1)

    NAME
        recycle - send files and directories to the Windows Recycle Bin

    SYNOPSIS
        recycle [OPTIONS] PATH...

    DESCRIPTION
        recycle safely moves target files and directories into the Windows
        Recycle Bin instead of permanently deleting them.

    OPTIONS
        -i, --interactive
            Prompt for confirmation before recycling each target.

        -v, --verbose
            Display detailed progress for each recycled item.

        -q, --quiet
            Suppress normal output; only display critical errors.

        --json
            Output results formatted as JSON.

        --csv
            Output results formatted as CSV.

        --table
            Output results formatted as an ASCII table.

        --pipe <command>
            Pipe formatted output through <command>.

        -h, --help
            Display this reference manual and exit.

        --version
            Display version information and exit.

    EXAMPLES
        recycle document.docx
            Send document.docx to the Recycle Bin.

        recycle -i file1.txt file2.txt
            Prompt for confirmation before recycling each file.

        recycle -v "C:\Temp\OldProject"
            Recycle a directory with verbose progress logging.

    CrossShell for UNIX                                                          recycle(1)
)";
    }

void OptionParser::PrintVersion() {
        std::wcout << L"recycle version 3.0.1\n";
        std::wcout << L"Copyright (C) 2026, Roberto J Dohnert.\n";
    }

RecycleOptions OptionParser::Parse(int argc, wchar_t* argv[]) const {
        RecycleOptions opts;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                opts.showHelp = true;
            } else if (arg == L"--version") {
                opts.showVersion = true;
            } else if (arg == L"-v" || arg == L"--verbose") {
                opts.verbose = true;
            } else if (arg == L"-q" || arg == L"--quiet") {
                opts.quiet = true;
            } else if (arg == L"-i" || arg == L"--interactive") {
                opts.interactive = true;
            } else if (arg == L"--json" || arg == L"--csv" || arg == L"--table") {
                opts.outputFormat = arg == L"--json" ? OutputFormat::Json : (arg == L"--csv" ? OutputFormat::Csv : OutputFormat::Table);
            } else if (arg == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"[ERROR] Unknown flag: " << arg << L"\n";
                opts.showHelp = true;
            } else {
                opts.targets.push_back(arg);
            }
        }
        return opts;
    }
