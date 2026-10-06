#include "options.hpp"

void OptionParser::PrintVersion() {
    std::wcout << PROG_NAME << L" version " << VERSION << L"\n"
               << L"Copyright (c) Roberto J Dohnert\n"
               << L"Licensed under the BSD 3-Clause License.\n";
}

void OptionParser::PrintHelp() {
    std::wcout << LR"(mvdir(1)                CrossShell for UNIX Reference Manual                  mvdir(1)

    NAME
        mvdir - move or rename directories

    SYNOPSIS
        mvdir [OPTIONS] DIRECTORY1 DIRECTORY2

    DESCRIPTION
        mvdir moves or renames Directory1 to Directory2.

        If Directory2 exists and is a directory, Directory1 is moved inside
        Directory2 as a subdirectory (Directory2\Directory1). If Directory2
        does not exist, Directory1 is renamed to Directory2, provided
        Directory2's parent directory exists.

        If Directory1 and Directory2 reside on different drives or volumes,
        a recursive copy-and-delete operation is automatically performed.

    OPTIONS
        -h, --help
            Display this reference manual.

        -v, --version
            Display version and license information.

    EXAMPLES
        mvdir oldname newname
            Rename directory oldname to newname.

        mvdir myproject D:\Archive\
            Move directory myproject into D:\Archive\.

        mvdir C:\Logs D:\LogsBackup
            Move directory tree across volumes.

    CrossShell for UNIX                                                    mvdir(1)
)";
}

bool OptionParser::Parse(int argc, wchar_t* argv[], MvdirOptions& opts) const {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
            opts.show_help = true;
            return true;
        }
        if (arg == L"--version" || arg == L"-v") {
            opts.show_version = true;
            return true;
        }
    }

    if (argc != 3) {
        std::wcerr << L"Usage: mvdir Directory1 Directory2\n"
                   << L"Try 'mvdir --help' for more information.\n";
        return false;
    }

    opts.source = PathValidator::NormalizeTrailingSeparator(argv[1]);
    opts.destination = PathValidator::NormalizeTrailingSeparator(argv[2]);
    return true;
}
