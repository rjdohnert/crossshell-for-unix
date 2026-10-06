#include "help_formatter.hpp"

void HelpFormatter::printUsage(std::ostream& os) {
        os << "Usage: rename [options] <expression> <replacement> <file...>\n"
           << "Try 'rename --help' for complete documentation and examples.\n";
    }

void HelpFormatter::printHelp(std::ostream& os) {
        os << R"HELP(
NAME
    rename - rename multiple files using pattern replacement

SYNOPSIS
    rename [OPTIONS] <expression> <replacement> <file...>
    rename -h | --help
    rename -V | --version

DESCRIPTION
    The rename utility renames the specified files by replacing matches of
    <expression> with <replacement> in each file's base name. The directory
    structure leading up to the file remains untouched.

    This program is a native Windows implementation compatible with the
    traditional Unix/HP-UX rename utility semantics, augmented with Windows
    command-line wildcard expansion and modern regular expression support.

OPTIONS
    -v, --verbose
        Display diagnostic output detailing every file renamed.

    -n, --no-act, --dry-run
        Simulate the renaming operations without committing any modifications
        to the filesystem. Use alongside -v to inspect pending operations.

    -i, --interactive
        Prompt for confirmation before overwriting an existing destination file.

    -o, --no-overwrite
        Do not overwrite existing destination files; collisions will be skipped.

    -a, --all
        Replace all occurrences of <expression> in the file name, rather than
        just the first occurrence.

    -l, --last
        Replace the last occurrence of <expression> rather than the first.
        (Ignored if combined with -a or -e).

    -c, --ignore-case
        Perform case-insensitive matching for <expression>.

    -e, --regex
        Treat <expression> as an ECMAScript regular expression. Capture groups
        such as $1, $2, etc., can be referenced in <replacement>.

    -h, --help
        Display this comprehensive manual page and exit.

    -V, --version
        Display utility version, build information, and exit.

WINDOWS SPECIFIC BEHAVIOR
    - Wildcard Expansion:
        Because Windows cmd.exe does not expand wildcards like Unix shells do,
        this utility automatically expands '*' and '?' wildcards across file
        arguments.

    - Case Renaming:
        Renaming a file to change its casing (e.g., lowercase to uppercase) is
        safely handled using native Win32 MoveFileEx replacement semantics.

    - Path Separators:
        Both forward ('/') and backward ('\\') slashes are supported as path
        separators.

EXIT STATUS
    0   Successful completion.
    1   One or more file renaming operations failed.
    2   Invalid command-line options or insufficient arguments.

EXAMPLES
    1. Replace the first occurrence of '.jpeg' with '.jpg' across files:
       > rename .jpeg .jpg *.jpeg

    2. Dry run: preview changing occurrences of 'draft' to 'final':
       > rename -v -n draft final draft_*.txt

    3. Replace ALL underscores with hyphens in file names:
       > rename -a "_" "-" *.*

    4. Replace the LAST occurrence of 'old' with 'new':
       > rename -l old new my_old_doc_old.txt

    5. Case-insensitive renaming with confirmation before overwriting:
       > rename -i -c data DATA data*.csv

    6. Regular expression capture and reordering:
       > rename -e "([a-z]+)_([0-9]+)" "$2_$1" *.log
)HELP";
    }

void HelpFormatter::printVersion(std::ostream& os) {
        os << "rename 1.2.0\n"
           << "Copyright (C) 2026, Roberto J. Dohnert.\n";
    }
