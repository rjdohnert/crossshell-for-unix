// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class JoinOptions {
public:
    int field1{1};
    int field2{1};
    wchar_t delim{L'\0'};
    bool includeUnpaired1{false};
    bool includeUnpaired2{false};
    bool unpairedOnly1{false};
    bool unpairedOnly2{false};
    std::wstring file1;
    std::wstring file2;
    int outputFormat{0};
    std::wstring pipeCommand;

    static void printHelp() {
        std::wcout << LR"(join(1)             CrossShell for UNIX Reference Manual                 join(1)

    NAME
        join - join lines of two files on a common field

    SYNOPSIS
        join [OPTIONS] FILE1 FILE2

    DESCRIPTION
        join pairs and merges lines from two sorted input files based on identical
        data in a specified join field. If FILE1 or FILE2 is '-', standard input
        is read.

    OPTIONS
        -1 FIELD
            Join on the specified 1-based FIELD of FILE1.

        -2 FIELD
            Join on the specified 1-based FIELD of FILE2.

        -j FIELD
            Equivalent to -1 FIELD -2 FIELD.

        -t CHAR
            Use CHAR as input and output field separator.

        -a FILENO
            Print unpairable lines from file FILENO (1 or 2).

        -v FILENO
            Like -a FILENO, but suppress successfully joined output lines.

        --json
            Output joined records formatted as JSON.

        --csv
            Output joined records formatted as CSV.

        --table
            Output joined records formatted as a table.

        --pipe COMMAND
            Send output through the specified pipe command.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        join file1.txt file2.txt
            Join lines on first whitespace-delimited field.

        join -t, -1 1 -2 2 customers.csv orders.csv
            Join CSV files matching field 1 in file 1 with field 2 in file 2.

        join -a 1 employees.txt departments.txt
            Include unpairable records from employees.txt.

    CrossShell for UNIX                                                    join(1)
)";
    }

    static void printVersion() {
        std::wcout << L"join 1.0.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], JoinOptions& opts) {
        std::vector<std::wstring> files;

        for (int i = 1; i < argc; ++i) {
            std::wstring a = argv[i];
            if (a == L"--help" || a == L"-h" || a == L"-help" || a == L"/?") {
                printHelp();
                std::exit(0);
            } else if (a == L"--version" || a == L"-V") {
                printVersion();
                std::exit(0);
            } else if (a == L"-1" && i + 1 < argc) {
                opts.field1 = _wtoi(argv[++i]);
            } else if (a == L"-2" && i + 1 < argc) {
                opts.field2 = _wtoi(argv[++i]);
            } else if (a == L"-j" && i + 1 < argc) {
                opts.field1 = opts.field2 = _wtoi(argv[++i]);
            } else if (a == L"-t" && i + 1 < argc) {
                std::wstring t = argv[++i];
                if (!t.empty()) opts.delim = t[0];
            } else if (a == L"-a" && i + 1 < argc) {
                int n = _wtoi(argv[++i]);
                if (n == 1) opts.includeUnpaired1 = true;
                if (n == 2) opts.includeUnpaired2 = true;
            } else if (a == L"-v" && i + 1 < argc) {
                int n = _wtoi(argv[++i]);
                if (n == 1) opts.unpairedOnly1 = true;
                if (n == 2) opts.unpairedOnly2 = true;
            } else if (a == L"--json") {
                opts.outputFormat = 1;
            } else if (a == L"--csv") {
                opts.outputFormat = 2;
            } else if (a == L"--table") {
                opts.outputFormat = 3;
            } else if (a == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (a[0] == L'-' && a.size() > 1) {
                std::wcerr << L"join: unknown option " << a << L"\n";
                return false;
            } else {
                files.push_back(a);
            }
        }

        if (files.size() != 2) {
            std::wcerr << L"join: requires exactly two files\n";
            return false;
        }

        opts.file1 = files[0];
        opts.file2 = files[1];
        return true;
    }
};

