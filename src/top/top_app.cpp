#include "process_monitor.hpp"
#include "top_app.hpp"

#include <charconv>

namespace {
bool ParsePositiveInteger(const char* text, int& value) {
    if (text == nullptr || *text == '\0') return false;
    const char* end = text;
    while (*end != '\0') ++end;
    auto result = std::from_chars(text, end, value);
    return result.ec == std::errc{} && result.ptr == end && value > 0;
}

bool ParseSortMode(const std::string& value, SortMode& mode) {
    if (value == "cpu") mode = SortMode::CPU;
    else if (value == "mem" || value == "memory") mode = SortMode::MEMORY;
    else if (value == "pid") mode = SortMode::PID;
    else if (value == "name") mode = SortMode::NAME;
    else if (value == "time") mode = SortMode::TIME;
    else return false;
    return true;
}

void PrintHelp() {
    std::cout << R"(top(1)                  CrossShell for UNIX Reference Manual                 top(1)

    NAME
        top - display process activity and system resource usage

    SYNOPSIS
        top [OPTIONS]

    DESCRIPTION
        top displays a continuously refreshed view of process and system
        activity in an interactive Windows console.

    OPTIONS
        -d, --delay SECONDS
        -i SECONDS
            Set the refresh delay in positive whole seconds (default: 1).

        -n ROWS
            Set the maximum number of process rows to display (default: 20).

        -s MODE
            Sort by cpu, mem, pid, name, or time (default: cpu).

        -u USER
            Show processes owned by the specified username.

        -h, --help
            Display this reference manual and exit.

        -V, --version
            Output version information and exit.

    INTERACTIVE COMMANDS
        h, ?        Display interactive help.
        q, Esc      Quit.
        Ctrl+X      Quit.
        k           Terminate a process by PID.
        d           Change the refresh delay.
        n           Change the number of process rows.
        u           Filter by username.
        c           Sort by CPU.
        m           Sort by resident memory.
        p           Sort by PID.
        a           Sort by command name.
        t           Sort by accumulated CPU time.

    EXAMPLES
        top -d 2 -n 30 -s mem
            Refresh every two seconds and show the 30 largest resident sets.

    CrossShell for UNIX                                                      top(1)
)";
}
}

int runTop(int argc, char* argv[]) {
    TopOptions options;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h" || arg == "/?" || arg == "-?") {
            PrintHelp();
            return 0;
        }
        if (arg == "--version" || arg == "-V" || arg == "-v") {
            std::cout << "top 1.0.0\r\n";
            return 0;
        }

        if (arg == "-d" || arg == "--delay" || arg == "-i" ||
            arg == "-n" || arg == "-s" || arg == "-u") {
            if (i + 1 >= argc) {
                std::cerr << "top: missing value for " << arg << "\r\n";
                return 2;
            }
            std::string value = argv[++i];
            if (arg == "-d" || arg == "--delay" || arg == "-i") {
                if (!ParsePositiveInteger(value.c_str(), options.refreshDelaySec)) {
                    std::cerr << "top: delay must be a positive whole number of seconds\r\n";
                    return 2;
                }
            } else if (arg == "-n") {
                if (!ParsePositiveInteger(value.c_str(), options.maxDisplayCount)) {
                    std::cerr << "top: row count must be a positive integer\r\n";
                    return 2;
                }
            } else if (arg == "-s") {
                if (!ParseSortMode(value, options.currentSort)) {
                    std::cerr << "top: sort mode must be cpu, mem, pid, name, or time\r\n";
                    return 2;
                }
            } else {
                options.userFilter = value;
            }
            continue;
        }

        std::cerr << "top: unknown option: " << arg << "\r\n";
        std::cerr << "Try 'top --help' for more information.\r\n";
        return 2;
    }

    HpUxTopEngine engine(options);
    return engine.Run();
}
