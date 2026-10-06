#include "options.hpp"

void PstreeOptions::PrintHelp() {
    std::wcout << LR"(pstree(1)                  CrossShell for UNIX Reference Manual                 pstree(1)

    NAME
        pstree - display a tree of processes

    SYNOPSIS
        pstree [OPTIONS] [PID]

    DESCRIPTION
        pstree displays running processes as a tree structure, illustrating
        parent-child relationships between active processes. If a PID is
        specified, the output is rooted at that process.

    OPTIONS
        -p, --show-pids
            Show process IDs (PIDs) in decimal alongside process names.

        -a, --arguments, --full-path
            Display full executable paths instead of just process names.

        -n, --numeric-sort
            Sort child processes numerically by PID instead of by name.

        -s, --show-parents
            Show ancestor processes of the specified PID up to the system root.

        -A, --ascii
            Use classic ASCII characters (|--, `--) to draw the tree.

        -U, --unicode
            Use Unicode box-drawing characters (├──, └──, │) to draw the tree.

        -c, --color
            Enable ANSI colorized output for process names and IDs.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        pstree
            Display the complete process tree in ASCII format.

        pstree -p
            Display process tree including process IDs.

        pstree -p -s 1234
            Display process 1234 and trace its ancestor chain to the root.

        pstree -A -n
            Display process tree with children sorted numerically by PID.

        pstree -U -c
            Display process tree using Unicode line drawing and ANSI colors.

    CrossShell for UNIX                                                          pstree(1)
)";
}

void PstreeOptions::PrintVersion() {
    std::wcout << L"pstree v" << VersionInfo::VERSION 
               << L" (" << VersionInfo::RELEASE_DATE << L")\n"
               << L"Copyright (C) 2026 " << VersionInfo::AUTHOR << L"\n";
}

bool PstreeOptions::ParseCommandLine(int argc, wchar_t* argv[], Config& config) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
            PrintHelp();
            exit(0);
        } else if (arg == L"-v" || arg == L"--version") {
            PrintVersion();
            exit(0);
        } else if (arg == L"-p" || arg == L"--show-pids") {
            config.showPids = true;
        } else if (arg == L"-a" || arg == L"--arguments" || arg == L"--full-path") {
            config.showFullPaths = true;
        } else if (arg == L"-n" || arg == L"--numeric-sort") {
            config.numericSort = true;
        } else if (arg == L"-s" || arg == L"--show-parents") {
            config.showParents = true;
        } else if (arg == L"-A" || arg == L"--ascii") {
            config.style = TreeStyle::ASCII;
        } else if (arg == L"-U" || arg == L"--unicode") {
            config.style = TreeStyle::Unicode;
        } else if (arg == L"-c" || arg == L"--color") {
            config.colorOutput = true;
        } else if (!arg.empty() && arg[0] != L'-') {
            try {
                size_t pos = 0;
                config.targetPid = std::stoul(arg, &pos);
                if (pos != arg.length()) {
                    std::wcerr << L"Error: Invalid PID parameter '" << arg << L"'. Must be a positive integer.\n";
                    return false;
                }
            } catch (...) {
                std::wcerr << L"Error: Invalid PID parameter '" << arg << L"'. Out of integer range.\n";
                return false;
            }
        } else {
            std::wcerr << L"Error: Unknown option '" << arg << L"'. Use 'pstree --help' for usage.\n";
            return false;
        }
    }
    return true;
}
