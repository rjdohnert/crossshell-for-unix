#include "options.hpp"

void PkgOptions::printHelp() {
    std::wcout << LR"(pkg(8)                  CrossShell for UNIX Reference Manual                  pkg(8)

    NAME
        pkg - package management utility for installing and managing applications

    SYNOPSIS
        pkg COMMAND [OPTIONS] [ARGUMENTS...]
        pkg [OPTIONS]

    DESCRIPTION
        pkg provides a unified BSD-style package management interface on Windows.
        It delegates package installation, search, querying, and upgrading to the
        Windows Package Manager (winget) subsystem while supporting direct local
        installation for MSI and MSIX software distributions.

    COMMANDS
        install, add PACKAGE...
            Install one or more packages by name or identifier.

        install --local FILE
            Install a local Windows installer package (.msi or .msix).

        delete, remove, rm PACKAGE...
            Uninstall one or more installed packages.

        search QUERY
            Search package repositories for applications matching QUERY.

        upgrade [PACKAGE...]
            Upgrade specified packages or all installed packages if none is given.

        info, list [PACKAGE]
            Display details for PACKAGE or enumerate all installed applications.

        update
            Update package manager repository sources and metadata catalogs.

        help
            Display this reference manual.

    OPTIONS
        --local FILE
            Install a local package file (.msi or .msix) directly.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        pkg install Git.Git
            Install Git using the Windows Package Manager repository.

        pkg install --local package.msix
            Install a local MSIX application package.

        pkg search python
            Search for available Python packages.

        pkg upgrade
            Upgrade all installed packages to their latest available versions.

        pkg delete Neovim.Neovim
            Uninstall Neovim.

        pkg info
            List all currently installed application packages.

    CrossShell for UNIX                                                   pkg(8)
)";
}

void PkgOptions::printVersion() {
    std::wcout << L"pkg (CrossShell) 5.0.0\n"
               << L"Copyright (c) 2026 Roberto J Dohnert. All rights reserved.\n";
}

bool PkgOptions::parse(int argc, wchar_t* argv[], PkgOptions& opts, bool& showHelp, bool& showVersion) {
    showHelp = false;
    showVersion = false;

    if (argc < 2) {
        showHelp = true;
        return false;
    }

    opts.command = argv[1];
    int start = 2;

    if (opts.command == L"--") {
        if (argc < 3) {
            showHelp = true;
            return false;
        }
        opts.command = argv[2];
        start = 3;
    }

    if (opts.command == L"help" || opts.command == L"-h" || opts.command == L"--help" || opts.command == L"/?" || opts.command == L"-?") {
        showHelp = true;
        return true;
    }

    if (opts.command == L"-V" || opts.command == L"-v" || opts.command == L"--version") {
        showVersion = true;
        return true;
    }

    for (int i = start; i < argc; ++i) {
        opts.arguments.push_back(argv[i]);
    }

    return true;
}
