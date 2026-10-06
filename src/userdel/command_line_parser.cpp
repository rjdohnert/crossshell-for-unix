#include "command_line_parser.hpp"

void CommandLineParser::PrintHelp() {
           std::cout << R"(userdel(1)              CrossShell for UNIX Reference Manual                   userdel(1)

    NAME
        userdel - delete a local Windows user account

    SYNOPSIS
        userdel [OPTIONS] LOGIN
        userdel --pipe [OPTIONS]
        COMMAND | userdel [OPTIONS]

    DESCRIPTION
        Deletes a local Windows user account and optionally removes its home
        directory and profile store. Piped input is detected automatically or can
        be selected explicitly with --pipe.

    OPTIONS
        -f, --force
            Force removal even when the user is logged in; terminate processes
            owned by the user.

        -r, --remove
            Remove the home directory and user profile store.

        -R, --root CHROOT_DIR
            Set the directory prefix used for home path resolution.

        -Z, --selinux-user
            POSIX compatibility option; ignored on Windows.

        --pipe
            Read account names to delete from standard input.

        -v, --verbose
            Enable detailed diagnostic and step-by-step output.

        -h, --help, /?
            Display this comprehensive reference manual and exit.

        --version
            Display version and licensing information.

    PIPING AND STREAMING
        Input may contain plain usernames, comma-separated values, or colon-delimited
        lists. Blank lines and comment records are ignored.

    EXIT STATUS
        0          Success.
        1          General failure or account not found.
        2          Invalid command syntax or usage.
        12         Access denied; administrative privileges are required.

    EXAMPLES
        userdel testuser
            Delete a local user without touching profile files.

        userdel -r testuser
            Delete a user and remove C:\Users\testuser.

        userdel -f -r loggedin_user
            Terminate the user's processes, delete the account, and remove its profile.

        type deprovision_list.txt | userdel -r --verbose
            Batch-delete users from a file.

        echo user1,user2,user3 | userdel -r --pipe
            Batch-delete comma-separated users from a pipeline.

    CrossShell for UNIX                                                     userdel(1)
    )";
    }

void CommandLineParser::PrintVersion() {
        std::cout << "userdel 2.0.0\n";
        std::cout << "Copyright (C) 2026, Roberto J Dohnert.\n";
    }
