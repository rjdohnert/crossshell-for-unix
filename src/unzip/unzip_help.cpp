#include "unzip_help.hpp"

void print_usage(const char* prog_name) {
    std::cout << R"(unzip(1)                CrossShell for UNIX Reference Manual                 unzip(1)

    NAME
        unzip - list, test and extract compressed files in a ZIP archive

    SYNOPSIS
        unzip [OPTIONS] FILE[.zip] [FILE...] [-d EXDIR]

    DESCRIPTION
        unzip will list, test, or extract files from a ZIP archive.

    OPTIONS
        -l
            List archive contents in short format.

        -t
            Test archive files integrity.

        -d EXDIR
            Extract files into directory EXDIR.

        -o
            Overwrite existing files without prompting.

        -f
            Freshen existing files (overwrite only if newer).

        -n
            Never overwrite existing files.

        -j
            Junk paths (extract all files flat into target directory).

        -q
            Quiet mode.

        --json, --csv, --table
            Format archive entry manifests as JSON, CSV, or table.

        --pipe COMMAND
            Stream extraction report to COMMAND.

        --help
            Display this reference manual.

    EXAMPLES
        unzip archive.zip -d out/
            Extract archive into out/ directory.

        unzip -l archive.zip --json
            List contents of archive in JSON format.

    CrossShell for UNIX                                                  unzip(1)
)";
}
