#include "su_help.hpp"

void ShowHelp() {
    std::wcout << LR"(su(1)                   CrossShell for UNIX Reference Manual                 su(1)

    NAME
        su - run a shell with substitute user and domain credentials

    SYNOPSIS
        su [OPTIONS] [USER]

    DESCRIPTION
        Switches to another user account or executes commands under substitute
        credentials using the Windows Logon API (CreateProcessWithLogonW).
        Prompts securely for account password without echoing. Default USER is
        'Administrator'.

    OPTIONS
        -l, -, --login
            Start the shell as a login shell.

        -c, --command COMMAND
            Pass COMMAND string to the invoked shell.

        -h, --help
            Display this reference manual.

    EXAMPLES
        su
            Switch to local Administrator with an interactive shell.

        su -l Developer
            Start a login shell as the Developer user.

        su -c "whoami /all" AdminUser
            Execute whoami command under the AdminUser credentials.

    CrossShell for UNIX                                                    su(1)
    )";
}
