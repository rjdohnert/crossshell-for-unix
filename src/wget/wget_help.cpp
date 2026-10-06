#include "wget_help.hpp"

void PrintUsage(const char* prog) {
    std::cout << R"(wget(1)                 CrossShell for UNIX Reference Manual                 wget(1)

    NAME
        wget - non-interactive network downloader

    SYNOPSIS
        wget [OPTIONS] URL [OUTPUT_FILE]

    DESCRIPTION
        Download files from the World Wide Web using HTTP or HTTPS.
        If OUTPUT_FILE is omitted, the filename is inferred from the URL.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        --json
            Emit a JSON success object.

        --csv
            Emit CSV status, URL, file, and byte fields.

        --table
            Emit tab-delimited status output.

        --pipe COMMAND
            Send textual output through pipeline COMMAND.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        wget https://example.com/archive.zip
            Download archive.zip to the current directory.

        wget --json https://example.com/data.json data.json
            Download data.json and output result in JSON format.

    CrossShell for UNIX                                                      wget(1)
)";
}
