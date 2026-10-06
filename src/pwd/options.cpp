#include "options.hpp"

void PwdOptions::printUsage() {
    std::wcout << LR"(pwd(1)                     CrossShell for UNIX Reference Manual                    pwd(1)

    NAME
        pwd - print name of current/working directory

    SYNOPSIS
        pwd [OPTIONS]

    DESCRIPTION
        pwd prints the full pathname of the current working directory to
        standard output.

    OPTIONS
        -L, --logical
            Use PWD from the environment, even if it contains symlinks (default).

        -P, --physical
            Resolve all symlinks and directory junctions to show the physical path.

        -h, --help
            Display this reference manual and exit.

        --version
            Display version information and exit.

    EXAMPLES
        pwd
            Print the current working directory path.

        pwd -P
            Print the physical path resolving any symbolic links or junctions.

    CrossShell for UNIX                                                          pwd(1)
)";
}

void PwdOptions::printVersion() {
    std::wcout << L"pwd 1.0.0\n";
}

bool PwdOptions::parse(int argc, wchar_t* argv[], PwdOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
            printUsage();
            std::exit(0);
        } else if (arg == L"--version") {
            printVersion();
            std::exit(0);
        } else if (arg == L"--logical") {
            opts.physical = false;
        } else if (arg == L"--physical") {
            opts.physical = true;
        } else if (arg[0] == L'-' && arg.size() > 1) {
            for (size_t j = 1; j < arg.size(); ++j) {
                switch (arg[j]) {
                    case L'L': opts.physical = false; break;
                    case L'P': opts.physical = true; break;
                    default:
                        std::wcerr << L"pwd: unknown option -- " << arg[j] << L"\n";
                        std::wcerr << L"usage: pwd [-L | -P]\n";
                        return false;
                }
            }
        } else {
            std::wcerr << L"pwd: too many arguments\n";
            std::wcerr << L"usage: pwd [-L | -P]\n";
            return false;
        }
    }
    return true;
}
