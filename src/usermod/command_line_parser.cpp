#include "command_line_parser.hpp"

void CommandLineParser::PrintHelp() {
           std::cout << R"(usermod(1)              CrossShell for UNIX Reference Manual                   usermod(1)

    NAME
        usermod - modify a local Windows user account

    SYNOPSIS
        usermod [OPTIONS] LOGIN
        usermod --pipe [OPTIONS]
        COMMAND | usermod --pipe

    DESCRIPTION
        Modifies the configuration of a local Windows user account. Changes may be
        supplied as command-line options or as key-value records through standard
        input in --pipe mode.

    OPTIONS
        -a, --append
            Append the user to supplementary groups specified with -G.

        -c, --comment COMMENT
            Set the user's comment or GECOS field.

        --full-name NAME
            Set the user's full display name.

        -d, --home-dir HOME_DIR
            Set the user's home directory.

        -m, --move-home
            Move the existing home directory contents to the new -d location.

        -g, --gid, --group GROUP
            Set the primary local group.

        -G, --groups GROUPS
            Set supplementary groups as a comma-separated list.

        -l, --login NEW_LOGIN
            Change the user's login name.

        -L, --lock
            Lock the password and disable the account.

        -U, --unlock
            Unlock the password and re-enable the account.

        -p, --password PASSWORD
            Assign a new cleartext password.

        -f, --force-password-change
            Require a password change at next logon.

        --never-expires
            Set the password to never expire.

        --expires
            Re-enable the password expiration schedule.

        --pipe
            Read records as <username>:<key>=<value>,<key>=<value>. Supported keys
            include comment, fullname, homedir, pass, groups, and lock.

        --stdin-password
            Read a single user's new password from standard input.

        -v, --verbose
            Enable detailed step-by-step diagnostic output.

        -h, --help, /?
            Display this comprehensive reference manual and exit.

        --version
            Display version and licensing information.

    EXIT STATUS
        0          Success.
        1          General failure or user account not found.
        2          Invalid command syntax or usage.
        12         Access denied; an elevated administrative shell is required.

    EXAMPLES
        usermod -l jsmith --full-name "John Smith" jdoe
            Rename an account and assign a new display name.

        usermod -L bad_actor
            Lock and disable a user account.

        usermod -d "D:\Profiles\jsmith" -m jsmith
            Move the existing home directory to a new location.

        usermod -a -G "Administrators,Remote Desktop Users" jsmith
            Append a user to two supplementary groups.

        powershell -Command "Read-Host -AsSecureString" | usermod --stdin-password jsmith
            Supply a password dynamically through standard input.

        echo jsmith:lock=true,comment=Suspended | usermod --pipe --verbose
            Apply batch modifications from a configuration stream.

    CrossShell for UNIX                                                     usermod(1)
    )";
    }

void CommandLineParser::PrintVersion() {
        std::cout << "usermod 2.0.0\n";
        std::cout << "Copyright (C) 2026, Roberto J Dohnert.\n";
    }
