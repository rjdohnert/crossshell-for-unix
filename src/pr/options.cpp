#include "options.hpp"

void PrOptions::printHelp() {
    std::wcout << LR"(pr(1)                      CrossShell for UNIX Reference Manual                     pr(1)

    NAME
        pr - paginate or columnate files for printing

    SYNOPSIS
        pr [OPTIONS] [FILE...]

    DESCRIPTION
        pr formats text files into paginated output with headers and page
        numbers for printing or viewing. If no FILE is specified or if FILE
        is '-', pr reads from standard input.

    OPTIONS
        -l <length>
            Set the page length to <length> lines (default: 66).

        -w <width>
            Set the page width to <width> columns (default: 72).

        -h <header>
            Use <header> in place of the filename in page headers.

        -t
            Omit page headers and trailers (suppress 5-line header/trailer).

        --json
            Output paginated data as JSON.

        --csv
            Output paginated data as CSV.

        --table
            Output paginated data formatted as an ASCII table.

        --pipe <command>
            Pipe paginated output through the specified shell command.

        --help
            Display this reference manual and exit.

        --version
            Display version information and exit.

    EXAMPLES
        pr file.txt
            Paginate file.txt with default 66-line pages and headers.

        pr -l 50 -h "Monthly Report" report.txt
            Format report.txt with 50-line pages and a custom header title.

        pr -t document.txt
            Print document.txt suppressing top and bottom headers.

    CrossShell for UNIX                                                          pr(1)
)";
}

void PrOptions::printVersion() {
    std::wcout << L"pr 1.0.0\n";
}

bool PrOptions::parse(int argc, wchar_t* argv[], PrOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--help" || a == L"-help" || a == L"/?") {
            printHelp();
            std::exit(0);
        } else if (a == L"--version") {
            printVersion();
            std::exit(0);
        } else if (a == L"-t") {
            opts.omitHeader = true;
        } else if (a == L"-l" && i + 1 < argc) {
            opts.pageLength = _wtoi(argv[++i]);
        } else if (a == L"-w" && i + 1 < argc) {
            opts.width = _wtoi(argv[++i]);
        } else if (a == L"-h" && i + 1 < argc) {
            opts.header = argv[++i];
        } else if (a == L"--json") {
            opts.outputFormat = 1;
        } else if (a == L"--csv") {
            opts.outputFormat = 2;
        } else if (a == L"--table") {
            opts.outputFormat = 3;
        } else if (a == L"--pipe" && i + 1 < argc) {
            opts.pipeCommand = argv[++i];
        } else if (a[0] == L'-' && a.size() > 1) {
            std::wcerr << L"pr: unknown option " << a << L"\n";
            return false;
        } else {
            opts.files.push_back(a);
        }
    }

    if (opts.files.empty()) {
        opts.files.push_back(L"-");
    }

    return true;
}
