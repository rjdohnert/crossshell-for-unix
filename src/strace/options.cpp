#include "options.hpp"
#include "reporter.hpp"

bool TraceOptions::Parse(int argc, wchar_t* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--help") {
            PrintHelp();
            return false;
        }
        if (arg == L"--version") {
            PrintVersion();
            return false;
        }
        if (arg == L"-f" || arg == L"-F" || arg == L"--follow") {
            followChildren = true;
            continue;
        }
        if (arg == L"-p" || arg == L"--pid") {
            if (i + 1 >= argc) {
                std::wcerr << L"strace: option requires an argument -- " << arg.substr(1) << std::endl;
                return false;
            }
            std::wstring pidText = argv[++i];
            attachToExistingProcess = true;
            targetPid = static_cast<DWORD>(std::wcstoul(pidText.c_str(), nullptr, 10));
            continue;
        }
        if (arg == L"-tt" || arg == L"--timestamp") {
            timestamp = true;
            continue;
        }
        if (arg == L"-v" || arg == L"--verbose") {
            verbose = true;
            continue;
        }
        if (arg == L"-q" || arg == L"-qq" || arg == L"--quiet") {
            quiet = true;
            continue;
        }
        if (arg == L"-c" || arg == L"--summary-only") {
            summaryOnly = true;
            continue;
        }
        if (arg == L"-i" || arg == L"--instruction-pointer") {
            printIp = true;
            continue;
        }
        if (arg == L"-s" || arg == L"--string-limit") {
            if (i + 1 >= argc) {
                std::wcerr << L"strace: option requires an argument -- " << arg.substr(1) << std::endl;
                return false;
            }
            std::wstring limitText = argv[++i];
            stringLimit = static_cast<DWORD>(std::wcstoul(limitText.c_str(), nullptr, 10));
            continue;
        }
        if (arg == L"-e" || arg == L"--events") {
            if (i + 1 >= argc) {
                std::wcerr << L"strace: option requires an argument -- " << arg.substr(1) << std::endl;
                return false;
            }
            std::wstring eventSpec = argv[++i];
            if (eventSpec.rfind(L"trace=", 0) == 0) {
                eventSpec = eventSpec.substr(6);
            }

            std::wstringstream stream(eventSpec);
            std::wstring token;
            filterEvents = true;
            while (std::getline(stream, token, L',')) {
                std::wstring cleaned = TraceFormatter::Trim(token);
                if (!cleaned.empty()) {
                    eventFilters.push_back(cleaned);
                }
            }
            if (eventFilters.empty()) {
                std::wcerr << L"strace: no event classes specified" << std::endl;
                return false;
            }
            continue;
        }
        if (arg == L"-o" || arg == L"--output") {
            if (i + 1 >= argc) {
                std::wcerr << L"strace: option requires an argument -- " << arg.substr(1) << std::endl;
                return false;
            }
            outputPath = argv[++i];
            continue;
        }
        if (arg == L"--json" || arg == L"--csv" || arg == L"--table") {
            outputFormat = (arg == L"--json") ? OutputFormat::Json
                : (arg == L"--csv" ? OutputFormat::Csv : OutputFormat::Table);
            continue;
        }
        if (arg == L"--pipe") {
            if (i + 1 >= argc) {
                std::wcerr << L"strace: option requires an argument -- pipe" << std::endl;
                return false;
            }
            pipeCommand = argv[++i];
            continue;
        }
        commandParts.push_back(arg);
    }

    if (!attachToExistingProcess && commandParts.empty()) {
        PrintHelp();
        return false;
    }

    return true;
}

void TraceOptions::PrintHelp() const {
    std::wcout << LR"(strace(1)               CrossShell for UNIX Reference Manual                  strace(1)

    NAME
        strace - trace process, thread, DLL, exception, and debug events

    SYNOPSIS
        strace [OPTIONS] COMMAND [ARGUMENTS...]
        strace [OPTIONS] --pid PID

    DESCRIPTION
        Launches a target under the Windows debugger or attaches to an existing
        process, then reports process creation, thread activity, DLL loading,
        debug strings, and exceptions.

    OPTIONS
        -f, --follow; -F
            Trace child processes created by the target.

        -p, --pid PID
            Attach to an existing process instead of launching a command.

        -tt, --timestamp
            Prefix event lines with a local timestamp.

        -v, --verbose
            Print additional detail for selected events.

        -q, -qq, --quiet
            Suppress informational banners and show only traced events.

        -e, --events LIST
            Restrict output to process, thread, dll, exception, or debug events.
            UNIX syntax such as trace=process,dll is also accepted.

        -o, --output FILE
            Write trace output to FILE instead of standard output.

        --json, --csv, --table
            Select JSON, CSV, or tabular output.

        --pipe COMMAND
            Send formatted output through COMMAND.

        -c, --summary-only
            Print time, call, and error summaries for each event class on exit.

        -i, --instruction-pointer
            Prefix trace lines with the instruction pointer address.

        -s, --string-limit LIMIT
            Set the maximum printed string size; the default is 32.

        --help
            Display this comprehensive reference manual and exit.

        --version
            Display version information and exit.

    NOTES
        Use cmd /c when tracing shell commands or pipelines. Native executables are
        best suited to debugger-based tracing.

    EXAMPLES
        strace cmd /c dir
        strace -f -tt cmd /c dir
        strace -F -q -e trace=process,thread notepad.exe
        strace -p 1234 -tt -e process,thread
        strace -e process,dll -v notepad.exe
        strace -o trace.txt notepad.exe
        strace -c cmd /c dir
        strace -i cmd /c dir
        strace -s 128 notepad.exe

    EXIT STATUS
        0          Successful tracing session.
        1          Help, version, parse, attachment, launch, or tracing setup failure.

    CrossShell for UNIX                                                       strace(1)
    )";
}

void TraceOptions::PrintVersion() const {
    std::wcout << L"strace v1.0.0\n";
}
