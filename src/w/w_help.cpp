#include "session_time_formatter.hpp"
#include "w_help.hpp"

void PrintHelp() {
    std::wcout << LR"(w(1)                    CrossShell for UNIX Reference Manual                    w(1)

    NAME
        w - show who is logged on and what they are doing

    SYNOPSIS
        w [OPTIONS] [USER]

    DESCRIPTION
        w displays information about the users currently logged on to the
        system and their active processes. The header displays the current
        local time, system uptime, total active logged-in user count, and
        CPU utilization. Each session record reports the user account name,
        terminal station (TTY), remote source address (FROM), login timestamp,
        idle duration, total session CPU time (JCPU), active process CPU time
        (PCPU), and the current foreground process (WHAT).

    OPTIONS
        -h, --no-header
            Suppress the system summary header line.

        -s, --short
            Use short output format; omit Login Time, JCPU, and PCPU columns.

        -f, --from
            Include the 'FROM' column displaying remote client IP or hostname.

        -l, --long
            Use long output format (default).

        -i, --ip-addr
            Display IP addresses instead of hostnames for remote sessions.

        --output FORMAT
            Select table, csv, tsv, or json output. The default is table.

        --json, -j, --csv, --tsv, --table
            Convenience shortcuts for structured output formats.

        --pipe COMMAND
            Stream formatted output directly to another command or utility.

        -?, -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    COLUMN DESCRIPTIONS
        User
            Logged-in Windows user account name.

        TTY
            Terminal Station name (Console, RDP-Tcp#0, etc.).

        FROM
            Remote IPv4/IPv6 client address or hostname.

        Login@
            Local timestamp when the user session logged in.

        Idle
            Duration since the user last interacted with the session.

        JCPU
            Total CPU time accumulated by all processes in the session.

        PCPU
            CPU time consumed by the active foreground process (WHAT).

        WHAT
            The active command or process running in the session.

    EXAMPLES
        w
            Display full active user session table and system summary.

        w -f
            Display session table with remote client IP addresses.

        w -s
            Display abbreviated summary format.

        w -h
            Display session table without the system summary header.

        w Administrator
            Display active session details for user 'Administrator' only.

        w --json
            Export active user session records formatted as JSON.

    CrossShell for UNIX                                                        w(1)
)";
}

void PrintVersion() {
    std::wcout << L"w (CrossShell) 5.0.0\n"
               << L"Copyright (c) 2026 Roberto J Dohnert. All rights reserved.\n";
}
