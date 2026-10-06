#include "help_formatter.hpp"

void printHelp() {
    std::cout <<
R"(rpm(1)                     CrossShell for UNIX Reference Manual                    rpm(1)

NAME
    rpm - query, install, upgrade, erase, and verify RPM packages

SYNOPSIS
    rpm [OPTIONS] [PACKAGE...]

DESCRIPTION
    Manages the RPM package database. Query operations inspect
    installed packages or RPM files; mutation operations install,
    upgrade, or erase packages.

OPTIONS
    Query Options:
        -q, --query
            Query package.
        -a, --all
            Query all installed packages.
        -i, --info
            Display package metadata including name, version, and summary.
        -l, --list
            List files in package.
        -p, --package
            Query an uninstalled package file.
        --whatprovides CAPABILITY
            Query package providing specific capability.
        --whatrequires CAPABILITY
            Query packages that require a capability.

    Install, Upgrade, and Erase Options:
        -i, --install
            Install a package.
        -U, --upgrade
            Upgrade a package (installs if not present).
        -e, --erase
            Erase (uninstall) a package.
        -v, --verbose
            Provide detailed progress and diagnostic output.
        -h, --hash
            Print 50 hash marks as package unpacks.
        --nodeps
            Do not verify package dependencies.
        --noscripts
            Do not execute package pre/post install scripts.
        --test
            Do not install, but tell if it would work.
        --prefix=DIR
            Relocate package to DIR if relocatable.
        --root=DIR
            Use DIR as top-level root directory.

    Verification Options:
        -V, --verify
            Verify a package installation.

    Common Options:
        -?, --help
            Display this comprehensive reference manual and exit.
        --version
            Display version information and exit.

EXAMPLES
    rpm -qa
        List all installed packages.

    rpm -qi package-name
        Display package metadata.

    rpm -ql package-name
        List files installed by a package.

    rpm -U package.rpm --test
        Validate an upgrade without extracting files.

    rpm -e package-name
        Erase an installed package.

EXIT STATUS
    0   Successful query, verification, or mutation.
    1   Invalid operation, missing package, dependency, privilege, lock,
        script, extraction, or verification failure.

    CrossShell for UNIX                                                   rpm(1)
)";
}

void printProgress(const std::string& pkgName) {
    std::cout << "\rUpdating / installing...   \n";
    std::cout << "1:" << std::setw(26) << std::left << pkgName.substr(0, 25) << " ";
    for (int i = 0; i < 50; ++i) {
        std::cout << "#";
        std::cout.flush();
        Sleep(5);
    }
    std::cout << " [100%]\n";
}
