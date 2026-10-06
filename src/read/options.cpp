#include "options.hpp"

void ReadOptions::printUsage() {
    std::wcout << LR"(read(1)                     CrossShell for UNIX Reference Manual                    read(1)

    NAME
        read - read a line from standard input

    SYNOPSIS
        read [OPTIONS]

    DESCRIPTION
        read reads a single line from standard input (or console) into memory
        and outputs the result. It supports prompts, silent input (password
        masking), character limits, timeouts, custom delimiters, and structured
        output formatting.

    OPTIONS
        -p, --prompt <prompt>
            Display <prompt> string on standard error before reading.

        -s, --silent
            Do not echo typed characters (silent/password mode).

        -r, --raw
            Raw mode; do not allow backslash to escape characters.

        -n <nchars>
            Return immediately after reading <nchars> characters.

        -t <timeout>
            Time out after <timeout> seconds (supports decimal values, e.g. 2.5).

        -d <delim>
            Read until the first character of <delim> instead of newline.

        --json
            Output the result as JSON.

        --csv
            Output the result as CSV.

        --table
            Output the result as an ASCII table.

        --pipe <command>
            Pipe structured output through <command>.

        -h, --help
            Display this reference manual and exit.

        -V, --version
            Display version information and exit.

    EXAMPLES
        read -p "Enter username: "
            Prompt user and read input line.

        read -s -p "Enter password: "
            Read password securely without echoing characters.

        read -n 1 -p "Continue? (y/n) "
            Read exactly one character.

        read -t 5.0 -p "Press Enter within 5 seconds: "
            Read with a 5-second timeout.

    CrossShell for UNIX                                                          read(1)
)";
}

void ReadOptions::printVersion() {
    std::wcout << L"read 1.0\n";
}

bool ReadOptions::parse(int argc, wchar_t* argv[], ReadOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
            printUsage();
            std::exit(0);
        } else if (arg == L"--version" || arg == L"-V") {
            printVersion();
            std::exit(0);
        } else if ((arg == L"-p" || arg == L"--prompt") && i + 1 < argc) {
            opts.prompt = argv[++i];
        } else if (arg == L"-s" || arg == L"--silent") {
            opts.silent = true;
        } else if (arg == L"-r" || arg == L"--raw") {
            opts.raw = true;
        } else if (arg == L"-n" && i + 1 < argc) {
            opts.maxChars = std::wcstol(argv[++i], nullptr, 10);
        } else if (arg == L"-t" && i + 1 < argc) {
            opts.timeoutSec = std::wcstod(argv[++i], nullptr);
        } else if (arg == L"-d" && i + 1 < argc) {
            std::wstring d = argv[++i];
            if (!d.empty()) opts.delim = d[0];
        } else if (arg == L"--json") {
            opts.outputFormat = ReadOutputFormat::Json;
        } else if (arg == L"--csv") {
            opts.outputFormat = ReadOutputFormat::Csv;
        } else if (arg == L"--table") {
            opts.outputFormat = ReadOutputFormat::Table;
        } else if (arg == L"--pipe" && i + 1 < argc) {
            opts.pipeCommand = argv[++i];
        }
    }
    return true;
}
