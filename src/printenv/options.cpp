#include "options.hpp"

void PrintenvOptions::printHelp() {
    std::wcout << LR"(printenv(1)                CrossShell for UNIX Reference Manual                printenv(1)

    NAME
        printenv - print all or part of environment

    SYNOPSIS
        printenv [OPTIONS] [VARIABLE...]

    DESCRIPTION
        printenv prints the values of the specified environment VARIABLE(s).
        If no VARIABLE is specified, it prints name and value pairs for all
        environment variables in the current process environment.

    OPTIONS
        -0, --null
            End each output line with NUL (0 byte) rather than newline.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

        --
            End of option processing. Subsequent arguments are treated as
            variable names even if they begin with a dash.

    EXAMPLES
        printenv
            Print all environment variables as KEY=VALUE lines.

        printenv PATH
            Print the value of the PATH environment variable.

        printenv USER PROFILE
            Print values of USER and PROFILE variables sequentially.

        printenv -0 PATH
            Print PATH terminated by a null character for scripting.

    CrossShell for UNIX                                                          printenv(1)
)";
}

void PrintenvOptions::printVersion() {
    std::wcout << L"printenv 1.0\n";
}

bool PrintenvOptions::parse(int argc, wchar_t* argv[], PrintenvOptions& opts) {
    bool parseOptions = true;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (parseOptions && arg == L"--") {
            parseOptions = false;
            continue;
        }

        if (parseOptions && (arg == L"-0" || arg == L"--null")) {
            opts.nullTerminated = true;
        } else if (parseOptions && (arg == L"-h" || arg == L"--help" || arg == L"/?")) {
            printHelp();
            std::exit(0);
        } else if (parseOptions && (arg == L"-v" || arg == L"--version")) {
            printVersion();
            std::exit(0);
        } else if (parseOptions && arg[0] == L'-' && arg.size() > 1) {
            std::wstring opt = arg.substr(1);
            if (opt == L"-null") {
                opts.nullTerminated = true;
            } else if (opt == L"-help") {
                printHelp();
                std::exit(0);
            } else if (opt == L"-version") {
                printVersion();
                std::exit(0);
            } else {
                std::wcerr << L"printenv: unrecognized option: " << arg << L"\n";
                printHelp();
                return false;
            }
        } else {
            opts.targets.push_back(arg);
        }
    }
    return true;
}
