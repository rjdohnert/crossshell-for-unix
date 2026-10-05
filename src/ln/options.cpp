#include "options.hpp"
#include <iostream>
#include <string>
#include <cwchar>

void OptionParser::PrintUsage() {
    std::wcout << LR"(ln(1)               CrossShell for UNIX Reference Manual                 ln(1)

NAME
    ln - make links between files

SYNOPSIS
    ln [OPTIONS] TARGET LINK_NAME
    ln [OPTIONS] TARGET... DIRECTORY

DESCRIPTION
    ln creates hard links by default, or symbolic links when -s is specified.
    It integrates with NTFS hard links and Windows symbolic links, automatically
    requesting SeCreateSymbolicLinkPrivilege when needed.

OPTIONS
    -s, --symbolic
        Make symbolic links instead of hard links.

    -f, --force
        Remove existing destination files unconditionally.

    -i, --interactive
        Prompt before removing existing destination files.

    -v, --verbose
        Print the name of each linked file.

    -n, --no-dereference
        Treat destination symbolic link to a directory as a normal file.

    -r, --relative
        Create symbolic links relative to link location.

    -F
        Allow removing existing destination directories.

    -h, --help
        Display this reference manual.

    -V, --version
        Display version and license information.

EXAMPLES
    ln -s target.txt symlink.txt
        Create a symbolic link named symlink.txt pointing to target.txt.

    ln file1.txt hardlink.txt
        Create an NTFS hard link named hardlink.txt.

    ln -s C:\Source\*.dll C:\App\bin\
        Link multiple files into a target directory.

CrossShell for UNIX                                                    ln(1)
)";
}

void OptionParser::PrintVersion() {
    std::wcout << L"ln 1.0.0\n";
}

bool OptionParser::Parse(int argc, wchar_t* argv[], LnOptions& opts) const {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"--") {
            for (++i; i < argc; ++i) {
                opts.files.push_back(argv[i]);
            }
            break;
        }

        if (arg == L"--help" || arg == L"-help" || arg == L"/?") {
            opts.show_help = true;
            return true;
        } else if (arg == L"--version" || arg == L"-V") {
            opts.show_version = true;
            return true;
        } else if (arg == L"-s" || arg == L"--symbolic") {
            opts.symbolic = true;
        } else if (arg == L"-f" || arg == L"--force") {
            opts.force = true;
        } else if (arg == L"-i" || arg == L"--interactive") {
            opts.interactive = true;
        } else if (arg == L"-v" || arg == L"--verbose") {
            opts.verbose = true;
        } else if (arg == L"-n" || arg == L"-h" || arg == L"--no-dereference") {
            opts.no_deref = true;
        } else if (arg == L"-F") {
            opts.force_dir = true;
        } else if (arg == L"-r" || arg == L"--relative") {
            opts.relative = true;
        } else if (arg.rfind(L"-", 0) == 0 && arg.length() > 1 && arg[1] != L'-') {
            for (size_t j = 1; j < arg.length(); ++j) {
                wchar_t c = arg[j];
                if (c == L's') opts.symbolic = true;
                else if (c == L'f') opts.force = true;
                else if (c == L'i') opts.interactive = true;
                else if (c == L'v') opts.verbose = true;
                else if (c == L'n' || c == L'h') opts.no_deref = true;
                else if (c == L'F') opts.force_dir = true;
                else if (c == L'r') opts.relative = true;
                else {
                    std::fwprintf(stderr, L"ln: invalid option -- '%c'\n", c);
                    return false;
                }
            }
        } else {
            opts.files.push_back(arg);
        }
    }

    return true;
}
