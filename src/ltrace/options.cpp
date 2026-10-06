#include "options.hpp"
#include <iostream>
#include <cstdlib>

void CommandLineParser::showHelp() {
    std::cout << R"(ltrace(1)                CrossShell for UNIX Reference Manual                 ltrace(1)

    NAME
        ltrace - trace dynamic library calls made by a process

    SYNOPSIS
        ltrace [OPTIONS] -- COMMAND [ARGUMENTS...]
        ltrace [OPTIONS] -p PID

    DESCRIPTION
        Intercepts and logs dynamic link library (DLL) function calls made by a
        process. Tracing can be attached to an existing process or applied while
        launching a command. Module filters and structured output are supported.

    OPTIONS
        -p, --pid <pid>
            Attach to an existing running process by process ID.

        -l, --lib <module>
            Restrict tracing to a DLL module, such as kernel32 or user32. May be
            specified multiple times.

        -o, --output <file>
            Append trace output to FILE.

        -v, --verbose
            Display diagnostic information about symbol hooking.

        --all-exports
            Disable the default unfiltered export hook cap.

        --no-color
            Disable ANSI color output.

        --json
            Emit machine-readable JSON output.

        --csv
            Emit CSV output.

        --table
            Emit tabular output.

        --pipe <command>
            Send formatted output to COMMAND.

        -h, --help
            Display this comprehensive reference manual and exit.

        -V, --version
            Display version information and exit.

        --
            End options and introduce the command and its arguments.

    EXAMPLES
        ltrace -- notepad.exe
            Launch Notepad and trace its dynamic library calls.

        ltrace -l kernel32 -l user32 -- notepad.exe
            Trace calls made specifically to kernel32.dll and user32.dll.

        ltrace -p 4812
            Attach to process 4812 and trace its API calls.

        ltrace -o trace_log.txt -l ntdll -- ping.exe 127.0.0.1
            Write filtered traces to a file for later analysis.

    EXIT STATUS
        0
            Help, version, or a completed tracing session.
        1
            Invalid options, failed attachment or launch, or pipe startup failure.

    CrossShell for UNIX                                                    ltrace(1)
)";
}

void CommandLineParser::showVersion() {
    std::cout << Color::BOLD << Color::B_CYAN << "ltrace" << Color::RESET << " version 1.1.0\n";
}

bool CommandLineParser::parse(int argc, char* argv[], Config& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            showHelp();
            std::exit(0);
        } else if (arg == "-V" || arg == "--version") {
            showVersion();
            std::exit(0);
        } else if (arg == "-p" || arg == "--pid") {
            if (i + 1 < argc) config.targetPid = std::strtoul(argv[++i], NULL, 10);
        } else if (arg == "-l" || arg == "--lib") {
            if (i + 1 < argc) config.filterModules.push_back(argv[++i]);
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) config.outputFile = argv[++i];
        } else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        } else if (arg == "--all-exports") {
            config.allExports = true;
        } else if (arg == "--no-color") {
            config.useColor = false;
        } else if (arg == "--json") {
            config.outputFormat = Config::OutputFormat::Json;
        } else if (arg == "--csv") {
            config.outputFormat = Config::OutputFormat::Csv;
        } else if (arg == "--table") {
            config.outputFormat = Config::OutputFormat::Table;
        } else if (arg == "--pipe") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --pipe requires a command\n";
                return false;
            }
            config.pipeCommand = argv[++i];
        } else if (arg == "--") {
            std::string cmd;
            for (int j = i + 1; j < argc; ++j) {
                cmd += argv[j];
                if (j + 1 < argc) cmd += " ";
            }
            config.commandLine = cmd;
            break;
        } else if (arg[0] == '-' && arg.length() > 1) {
            std::cerr << Color::RED << "Error: Unknown option '" << arg << "'" << Color::RESET << "\n";
            std::cerr << "For usage information, run: ltrace --help\n";
            return false;
        } else {
            if (config.commandLine.empty()) {
                config.commandLine = arg;
            }
        }
    }

    if (config.targetPid == 0 && config.commandLine.empty()) {
        std::cerr << Color::RED << "Error: No target program or PID specified." << Color::RESET << "\n";
        std::cerr << "For usage information, run: ltrace --help\n";
        return false;
    }

    return true;
}
