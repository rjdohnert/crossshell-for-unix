#include "reporter.hpp"

void PurgeReporter::DisplayHelp(const std::string& topic, std::ostream& out) {
    out << R"HELP(purge(1)                CrossShell for UNIX Reference Manual                   purge(1)

    NAME
        purge - remove older versions of files

    SYNOPSIS
        purge [FILESPEC] [OPTIONS]
        purge [HELP_TOPIC]

    DESCRIPTION
        Groups matching files by VMS-style suffixes, numeric extensions, version
        tags, or timestamps and deletes older revisions. With no filespec, the
        current directory is scanned using '*'.

    OPTIONS
        /KEEP=N, /K:N, -k N, --keep=N
            Retain the newest N versions; the default is 1.
        /LOG, /L, -l, --log
            Report deleted files and sizes.
        /CONFIRM, /C, -c, --confirm
            Prompt before each deletion.
        /ERASE, /E, -e, --erase
            Overwrite with random data and zeros before deletion.
        /EXCLUDE=PATTERN, -x PATTERN, --exclude=PATTERN
            Skip files matching PATTERN.
        /GRAND_TOTAL, /G, -g, --grand-total
            Display a cumulative deletion summary.
        /OUTPUT=FILE, -o FILE, --output=FILE
            Redirect report output to FILE.
        /HELP [TOPIC], /?, -h, --help [TOPIC]
            Display general or topic-specific help.

    HELP TOPICS
        QUALIFIERS, PARAMETERS, DESCRIPTION, EXAMPLES

    EXAMPLES
        purge /KEEP=2 /LOG
        purge *.bak -k 1 --erase --log
        purge /CONFIRM /EXCLUDE=*.SYS

    EXIT STATUS
        0          No matching files or completed processing.
        1          Help or normal purge completion as implemented.
        2          Output-file or directory failure.
        4          Directory enumeration failure.

    CrossShell for UNIX                                                       purge(1)
)HELP";
}
