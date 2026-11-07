#include "gzip_cli.hpp"

#include <iostream>

void GzipCli::printHelp() {
    std::wcout << LR"HELP(gzip(1)                 CrossShell for UNIX Reference Manual                  gzip(1)

NAME
    gzip - compress or decompress gzip files

SYNOPSIS
    gzip [OPTIONS] FILE...
    gzip -c [OPTIONS] [FILE...]
    gzip [OPTIONS] < input > output

DESCRIPTION
    Compress files to FILE.gz or decompress gzip files. File inputs are
    removed only after the output has been validated and installed; use -k
    to preserve them. With -c, or when reading standard input, output is
    written to standard output and input files are kept.

OPTIONS
    -d, --decompress, --uncompress
        Decompress input files.

    -k, --keep
        Keep input files after successful file-to-file processing.

    -f, --force
        Replace an existing regular output file.

    -c, --stdout
        Write compressed or decompressed bytes to standard output.

    --json, --csv, --table
        Report completed file operations in the selected format.

    --pipe COMMAND
        Send the operation report to COMMAND.

    -h, --help
        Display this help.

    -V, --version
        Display version information.

EXAMPLES
    gzip archive.tar
        Compress archive.tar to archive.tar.gz and remove the input.

    gzip -k report.txt
        Compress report.txt while preserving the input.

    gzip -dc archive.tar.gz > archive.tar
        Decompress to standard output without removing the input.

    gzip --json -k report.txt
        Keep report.txt and report the operation as JSON.

CrossShell for UNIX                                                        gzip(1)
)HELP";
}

void GzipCli::printVersion() {
    std::wcout << L"gzip 1.0.0 (CrossShell for UNIX)\n";
}

bool GzipCli::parse(int argc, wchar_t* argv[], GzipOptions& options,
                    std::wstring& error) {
    options = GzipOptions{};
    error.clear();
    bool positionalOnly = false;

    for (int i = 1; i < argc; ++i) {
        const std::wstring argument = argv[i] == nullptr ? L"" : argv[i];
        if (argument.empty()) {
            error = L"empty arguments are not valid file names";
            return false;
        }
        if (!positionalOnly && argument == L"--") {
            positionalOnly = true;
            continue;
        }
        if (!positionalOnly &&
            (argument == L"-h" || argument == L"--help" || argument == L"/?")) {
            options.showHelp = true;
            continue;
        }
        if (!positionalOnly && (argument == L"-V" || argument == L"--version")) {
            options.showVersion = true;
            continue;
        }
        if (!positionalOnly &&
            (argument == L"-d" || argument == L"--decompress" ||
             argument == L"--uncompress")) {
            options.decompress = true;
            continue;
        }
        if (!positionalOnly && (argument == L"-k" || argument == L"--keep")) {
            options.keep = true;
            continue;
        }
        if (!positionalOnly && (argument == L"-f" || argument == L"--force")) {
            options.force = true;
            continue;
        }
        if (!positionalOnly && (argument == L"-c" || argument == L"--stdout")) {
            options.stdoutMode = true;
            continue;
        }
        if (!positionalOnly && argument == L"--json") {
            options.reportFormat = ReportFormat::json;
            continue;
        }
        if (!positionalOnly && argument == L"--csv") {
            options.reportFormat = ReportFormat::csv;
            continue;
        }
        if (!positionalOnly && argument == L"--table") {
            options.reportFormat = ReportFormat::table;
            continue;
        }
        if (!positionalOnly && argument == L"--pipe") {
            if (i + 1 >= argc || argv[i + 1] == nullptr || argv[i + 1][0] == L'\0') {
                error = L"--pipe requires a command";
                return false;
            }
            options.pipeCommand = argv[++i];
            continue;
        }
        if (!positionalOnly && argument.size() > 1 && argument[0] == L'-') {
            for (size_t j = 1; j < argument.size(); ++j) {
                switch (argument[j]) {
                    case L'd': options.decompress = true; break;
                    case L'k': options.keep = true; break;
                    case L'f': options.force = true; break;
                    case L'c': options.stdoutMode = true; break;
                    default:
                        error = L"unknown option: ";
                        error += argument.substr(j - 1);
                        return false;
                }
            }
            continue;
        }
        options.files.emplace_back(argument);
    }

    if (options.showHelp && options.showVersion) {
        error = L"help and version cannot be requested together";
        return false;
    }
    return true;
}
