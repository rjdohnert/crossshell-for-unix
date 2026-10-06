#include "options.hpp"

bool TimeOptions::Parse(int argc, wchar_t* argv[]) {
    cmdStart = argc;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--help") {
            showHelp = true;
            return true;
        } else if (arg == L"--version") {
            showVersion = true;
            return true;
        } else if (arg == L"--") {
            cmdStart = i + 1;
            break;
        } else if (arg == L"-p" || arg == L"--posix") {
            posixFormat = true;
        } else if (arg == L"-l" || arg == L"--detailed") {
            detailedInfo = true;
        } else if (arg == L"-h" || arg == L"--human") {
            humanReadable = true;
        } else if (arg == L"-a" || arg == L"--append") {
            appendOutput = true;
        } else if ((arg == L"-o" || arg == L"--output") && i + 1 < argc) {
            outputFile = argv[++i];
        } else if (arg.rfind(L"-", 0) == 0 && arg.length() > 1 && arg[1] != L'-') {
            bool valid = true;
            for (size_t j = 1; j < arg.length(); ++j) {
                wchar_t c = arg[j];
                if (c == L'p') posixFormat = true;
                else if (c == L'l') detailedInfo = true;
                else if (c == L'h') humanReadable = true;
                else if (c == L'a') appendOutput = true;
                else { valid = false; break; }
            }
            if (!valid) {
                cmdStart = i;
                break;
            }
        } else {
            cmdStart = i;
            break;
        }
    }

    return true;
}

void TimeOptions::PrintHelp() const {
    std::wcout << LR"(ptime(1)                   CrossShell for UNIX Reference Manual                  ptime(1)

    NAME
        ptime - measure command execution time and resource utilization

    SYNOPSIS
        ptime [OPTIONS] [--] COMMAND [ARGS...]

    DESCRIPTION
        ptime executes the specified COMMAND with optional ARGS, measures its
        elapsed real time, user CPU time, and system CPU time, and writes
        timing and resource statistics to standard error (or to a specified
        file).

    OPTIONS
        -p, --posix
            Use POSIX-standard timing format (real, user, sys on separate lines).

        -l, --detailed
            Report detailed resource utilization statistics (memory, page faults,
            process counts, and I/O operations).

        -h, --human
            Format elapsed time in human-readable notation (e.g., 1m23.456s).

        -o, --output <file>
            Write timing statistics to <file> instead of standard error.

        -a, --append
            Append timing output to the file specified with -o.

        --
            End option processing. Subsequent arguments are treated as the command
            and its arguments even if they begin with a dash.

        --help
            Display this reference manual and exit.

        --version
            Display version information and exit.

    EXAMPLES
        ptime cmd /c dir
            Measure execution time of directory listing.

        ptime -p -l -- myapp.exe -v
            Time myapp.exe in POSIX format with detailed memory and I/O statistics.

        ptime -o timing.log -a -- build.bat
            Append build execution times to timing.log.

    CrossShell for UNIX                                                          ptime(1)
)";
}

void TimeOptions::PrintVersion() const {
    std::wcout << L"ptime 1.0.0\n";
}
