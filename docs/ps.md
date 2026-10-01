# ps

## What it does

Shows information about running processes.

## Command Options

Displays running processes in the system.\n\n"
- Syntax
- ps qualifiers
- Qualifiers (accepts VMS-style /QUALIFIER or UNIX-style --qualifier / -qualifier)
- /HELP, --help, -h, /?              Display this help information.
- /HEADING, --heading                Display the system header and table column labels (Default).
- /NOHEADING, --noheading            Suppress the system header and column labels.
- /BATCH, --batch                    Show only script/batch/background processes (Session 0 services).
- /NOBATCH, --nobatch                Exclude batch/background processes from the display.
- /INTERACTIVE, --interactive        Show only interactive processes (Session 1+ user processes).
- /NOINTERACTIVE, --nointeractive    Exclude interactive processes from the display.
- /PROCESS=name, --process=name      Filter output to show only processes matching a specific name.
                                     Supports wildcards '*' and '?'.
- /IDENT=pid, --ident=pid            Filter output to show only the process with the given PID
                                     PID can be specified in decimal or 0x hex format.
- /IMAGE, --image                    Append the full executable path next to each process row.
- /FULL, --full                      Display detailed multi-line process status blocks.
                                     including PPID, threads, and owner username.
- /TOTAL, --total                    Print a summary line showing the total count of matching processes.
- /GRAND_TOTAL, --grand_total        Print a grand total summary line at the end.

Note: Both VMS-style slash qualifiers and UNIX-style hyphen qualifiers are fully supported.

## UNIX origin

A fundamental Unix process-monitoring utility from early BSD and System V releases follows the style of SHOW SYSTEM from VAX/VMS.
