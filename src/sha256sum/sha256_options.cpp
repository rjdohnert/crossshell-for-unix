#include "sha256_options.hpp"

void Sha256Options::printUsage(const char* progName) {
    (void)progName;
    std::cout << R"(sha256sum(1)            CrossShell for UNIX Reference Manual           sha256sum(1)

    NAME
        sha256sum - compute and check SHA-256 message digests

    SYNOPSIS
        sha256sum [OPTIONS] [FILE]...

    DESCRIPTION
        Computes and verifies SHA-256 (256-bit) checksums according to FIPS 180-4.
        Reads standard input when FILE is '-' or omitted. Output can be formatted
        as plain checksum lines or structured JSON/CSV/Table records.

    OPTIONS
        -b, --binary
            Read files in binary mode (default on Windows).

        -t, --text
            Read files in text mode.

        -c, --check
            Read SHA256 sums from the FILEs and check them.

        --status
            Do not output anything; status code shows success.

        -q, --quiet
            Do not print OK for each successfully verified file.

        -w, --warn
            Warn about improperly formatted checksum lines.

        --json
            Emit checksum results in JSON format.

        --csv
            Emit checksum results in CSV format.

        --table
            Emit checksum results in tabular format.

        --pipe COMMAND
            Stream results directly into COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Display version information and exit.

    EXAMPLES
        sha256sum file.iso
            Compute SHA-256 digest for file.iso.

        sha256sum -c checksums.txt
            Verify checksums listed in checksums.txt.

        sha256sum --json *.dll
            Output SHA-256 digests in structured JSON format.

    CrossShell for UNIX                                                      sha256sum(1)
)";
}

void Sha256Options::printVersion() {
    std::cout << "sha256sum 1.0.0\n";
}

bool Sha256Options::parse(int argc, char* argv[], Sha256Options& opts) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "--version") {
            printVersion();
            std::exit(0);
        } else if (arg == "-b" || arg == "--binary") {
            opts.binaryMode = true;
        } else if (arg == "-t" || arg == "--text") {
            opts.binaryMode = false;
        } else if (arg == "-c" || arg == "--check") {
            opts.doCheck = true;
        } else if (arg == "--status") {
            opts.status = true;
        } else if (arg == "-q" || arg == "--quiet") {
            opts.quiet = true;
        } else if (arg == "-w" || arg == "--warn") {
            opts.warn = true;
        } else if (arg == "--json") {
            opts.outputFormat = 1;
        } else if (arg == "--csv") {
            opts.outputFormat = 2;
        } else if (arg == "--table") {
            opts.outputFormat = 3;
        } else if (arg == "--pipe" && i + 1 < argc) {
            opts.pipeCommand = argv[++i];
        } else if (!arg.empty() && arg[0] == '-' && arg != "-") {
            std::cerr << "sha256sum: invalid option '" << arg << "'\n";
            return false;
        } else {
            opts.files.push_back(arg);
        }
    }

    if (opts.files.empty()) {
        opts.files.push_back("-");
    }

    return true;
}
