#include "process_monitor.hpp"
#include "top_app.hpp"

int runTop(int argc, char* argv[]) {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--help" || arg == "-h" || arg == "/?" || arg == "-?") {
            std::cout << R"(top(1)                  CrossShell for UNIX Reference Manual                 top(1)

    NAME
        top - display Linux / HP-UX process activity and system resource monitor

    SYNOPSIS
        top [OPTIONS]

    DESCRIPTION
        top provides an ongoing look at processor activity in real time. It
        displays a listing of the most CPU-intensive tasks on the system.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -d DELAY
            Specifies the delay between screen updates in seconds.

        -n COUNT
            Specifies the maximum number of iterations or processes to show.

        -u USER
            Monitor only processes with the specified user ID or username.

        -h, --help
            Display this reference manual and exit.

        -V, --version
            Output version information and exit.

    INTERACTIVE COMMANDS
        h, ?        Display interactive help screen.
        k           Kill a process by PID.
        d           Change refresh delay interval.
        n           Change number of processes shown.
        u           Filter by username.
        q           Quit.

    EXAMPLES
        top
            Launch interactive process monitor.

    CrossShell for UNIX                                                      top(1)
)";
            return 0;
        }
        if (arg == "--version" || arg == "-V" || arg == "-v") {
            std::cout << "top 1.0.0\n";
            return 0;
        }
    }
    // Process arguments
    HpUxTopEngine engine;
    engine.Run();
    return 0;
}
