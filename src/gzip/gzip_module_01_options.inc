// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class GzipOptions {
public:
    bool keep{false};
    bool force{false};
    bool decompress{false};
    std::vector<std::wstring> files;
    int outputFormat{0};
    std::wstring pipeCommand;

    static void printUsage(const wchar_t* /*prog*/) {
        std::wcout << LR"HELP(gzip(1)                 CrossShell for UNIX Reference Manual                  gzip(1)

    NAME
        gzip - Compress or decompress gzip files.

    SYNOPSIS
        gzip [OPTIONS] FILE...

    DESCRIPTION
        Compresses each FILE to FILE.gz, or decompresses gzip files when
        decompression mode is selected. Input files are removed after a
        successful operation unless --keep is specified. Compression is
        performed through the Windows PowerShell GZipStream implementation.

    OPTIONS
        -d, --decompress, --uncompress
            Decompress each input file. A .gz suffix is removed when present;
            otherwise the output file receives an .out suffix.

        -k, --keep
            Keep input files after successful compression or decompression.

        -f, --force
            Overwrite an existing output file.

        --json
            Format the completed-operation report as JSON.

        --csv
            Format the completed-operation report as CSV.

        --table
            Format the completed-operation report as a table.

        --pipe COMMAND
            Send the formatted operation report through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version information.

    EXAMPLES
        gzip archive.tar
            Compress archive.tar to archive.tar.gz and remove the input.

        gzip -k report.txt
            Compress report.txt while preserving the input file.

        gzip -d archive.tar.gz
            Decompress archive.tar.gz to archive.tar.

        gzip --json -k report.txt
            Keep report.txt and report the compression operation as JSON.

    CrossShell for UNIX                                                        gzip(1)
    )HELP";
    }

    static void printVersion() {
        std::wcout << L"gzip 1.0.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], GzipOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == L"-V" || arg == L"--version") {
                printVersion();
                std::exit(0);
            } else if (arg == L"-d" || arg == L"--decompress" || arg == L"--uncompress") {
                opts.decompress = true;
            } else if (arg == L"-k" || arg == L"--keep") {
                opts.keep = true;
            } else if (arg == L"-f" || arg == L"--force") {
                opts.force = true;
            } else if (arg == L"--json") {
                opts.outputFormat = 1;
            } else if (arg == L"--csv") {
                opts.outputFormat = 2;
            } else if (arg == L"--table") {
                opts.outputFormat = 3;
            } else if (arg == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg[0] == L'-' && arg.size() > 1) {
                for (size_t j = 1; j < arg.size(); ++j) {
                    if (arg[j] == L'd') opts.decompress = true;
                    else if (arg[j] == L'k') opts.keep = true;
                    else if (arg[j] == L'f') opts.force = true;
                    else {
                        std::wcerr << L"gzip: unknown option -" << arg[j] << L"\n";
                        return false;
                    }
                }
            } else {
                opts.files.push_back(arg);
            }
        }

        if (opts.files.empty()) {
            std::wcerr << L"gzip: no files specified\n";
            return false;
        }

        return true;
    }
};

