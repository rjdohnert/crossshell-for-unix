class FileOptions {
public:
    bool brief{false};
    bool mime{false};
    std::vector<std::wstring> files;
    int outputFormat{0};
    std::wstring pipeCommand;

    static void printHelp() {
        std::wcout << LR"(file(1)             CrossShell for UNIX Reference Manual                 file(1)

    NAME
        file - determine file type

    SYNOPSIS
        file [OPTIONS] FILE...

    DESCRIPTION
        file inspects file system properties, reparse points, headers, and magic
        signatures to determine the classification and type of the specified files.
        It supports detection of Windows PE executables, ELF binaries, archive formats,
        images, documents, and plain or encoded text files.

    OPTIONS
        -b, --brief
            Do not prepend filenames to the classification output.

        -i, --mime
            Output MIME type strings instead of human-readable text.

        --json
            Output file classification metadata in JSON format.

        --csv
            Output file classification metadata in CSV format.

        --table
            Output file classification metadata in tabular format.

        --pipe COMMAND
            Send classification output through the specified pipe command.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        file application.exe
            Determine the type of application.exe.

        file --brief --mime document.pdf
            Display only the MIME type for document.pdf.

        file --json *.dll
            Classify all DLLs in the directory and output structured JSON.

    CrossShell for UNIX                                                    file(1)
)";
    }

    static void printVersion() {
        std::wcout << L"file 1.0.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], FileOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::wstring a = argv[i];
            if (a == L"--help" || a == L"-help" || a == L"-h" || a == L"/?") {
                printHelp();
                std::exit(0);
            } else if (a == L"--version" || a == L"-V") {
                printVersion();
                std::exit(0);
            } else if (a == L"-b" || a == L"--brief") {
                opts.brief = true;
            } else if (a == L"-i" || a == L"--mime") {
                opts.mime = true;
            } else if (a == L"--json") {
                opts.outputFormat = 1;
            } else if (a == L"--csv") {
                opts.outputFormat = 2;
            } else if (a == L"--table") {
                opts.outputFormat = 3;
            } else if (a == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (a[0] == L'-' && a.size() > 1) {
                std::wcerr << L"file: unknown option " << a << L"\n";
                return false;
            } else {
                opts.files.push_back(a);
            }
        }

        if (opts.files.empty()) {
            std::wcerr << L"file: missing argument\n";
            return false;
        }

        return true;
    }
};
