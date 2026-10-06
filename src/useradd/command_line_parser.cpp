#include "command_line_parser.hpp"

void CommandLineParser::PrintHelp() {
           std::cout << R"(useradd(1)              CrossShell for UNIX Reference Manual                   useradd(1)

    NAME
        useradd - create a local Windows user account

    SYNOPSIS
        useradd [OPTIONS] LOGIN
        useradd --stdin [OPTIONS]
        COMMAND | useradd --pipe

    DESCRIPTION
        Creates a local Windows user account or batch-creates accounts from standard
        input. User entries may be supplied through the explicit --pipe mode or the
        --stdin-password option.

    OPTIONS
        -c, --comment COMMENT
            Set the user's full comment or description.

        -d, --home-dir HOME_DIR
            Set the home directory path for the new account.

        -e, --expiredate EXPIRE_DATE
            Set an expiration date in YYYY-MM-DD format, or NEVER.

        -f, --force-password-change
            Require the user to change the password at next logon.

        -g, --gid, --group GROUP
            Set the primary group; the default is Users.

        -G, --groups GROUPS
            Set supplementary groups as a comma-separated list.

        -k, --skel SKEL_DIR
            Use a custom skeleton directory; ignored when -M is set.

        -m, --create-home
            Create the user's home directory (default).

        -M, --no-create-home
            Do not create the user's home directory.

        -N, --no-user-group
            Do not create a group with the same name as the user.

        -p, --password PASSWORD
            Set an encrypted or cleartext password.

        -r, --system
            Create a system account.

        -s, --shell SHELL
            Set the login shell, such as powershell.exe, cmd.exe, or bash.exe.

        -u, --uid UID
            Set a numeric user identifier or SID alias.

        -U, --user-group
            Create a group with the same name as the user.

        --disabled
            Create the account in a disabled state.

        --never-expires
            Set the password to never expire.

        --pipe
            Read entries as <username>:<password>:[comment]:[group1,group2].

        --stdin-password
            Read a single user's password from standard input.

        -v, --verbose
            Enable verbose diagnostic output.

        -h, --help, /?
            Display this comprehensive reference manual and exit.

        --version
            Display version and licensing information.

    EXAMPLES
        useradd -m -p "P@ssw0rd123!" -G Administrators,Users -c "Jane Doe" jdoe
            Create a user with a password and supplementary groups.

        useradd -M -r --disabled -c "CI Runner Service" svc_runner
            Create a disabled service account without a home directory.

        echo alice:P@ss1:Alice Walker:Developers | useradd --pipe
            Batch-create an account from a colon-delimited input record.

        type users.txt | useradd --pipe --verbose
            Batch-create accounts from a file.

        powershell -Command "Read-Host -AsSecureString" | useradd -m --stdin-password devuser
            Supply a password through a standard input pipeline.

    CrossShell for UNIX                                                     useradd(1)
    )";
    }

void CommandLineParser::PrintVersion() {
        std::cout << "useradd 2.0.0\n";
        std::cout << "Copyright (C) 2026, Roberto J Dohnert.\n";
    }
