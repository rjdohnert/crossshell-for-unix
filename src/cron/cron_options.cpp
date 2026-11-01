#include "cron_options.h"

void CronOptions::printUsage(const char* prog) {
    (void)prog;
    std::cout << R"(cron(1)                 CrossShell for UNIX Reference Manual                  cron(1)

    NAME
        cron - run scheduled Windows crontab jobs

    SYNOPSIS
        cron [OPTIONS] [CRONTAB_PATH]

    DESCRIPTION
        Loads crontab.txt by default and evaluates five-field cron schedules.
        Blank lines and comments are ignored; wildcards, lists, ranges, steps,
        named months/days (JAN-DEC, SUN-SAT), and @macros (@hourly, @daily,
        @weekly, @monthly, @yearly, @reboot) are supported.

        When both day-of-month and day-of-week are restricted, a job runs when
        EITHER matches (standard cron OR semantics).

    OPTIONS
        --system-crontab
            Store and read from C:\cron\crontab.txt.

        --json, --csv, --table
            Emit daemon status records in JSON, CSV, or tabular format.

        --pipe COMMAND
            Send daemon status directly through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

        --
            End options before CRONTAB_PATH.

    EXAMPLES
        cron
            Run cron daemon using default crontab.txt.

        cron C:\cron\production.txt
            Run cron daemon using specified crontab file.

        cron --system-crontab --json
            Run system crontab and emit JSON status.

    CrossShell for UNIX                                                    cron(1)
)";
}

void CronOptions::printVersion() {
    std::cout << "cron 1.1.0\n";
}

bool CronOptions::parse(int argc, char* argv[], CronOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--json") { opts.outputFormat = 1; continue; }
        if (arg == "--csv") { opts.outputFormat = 2; continue; }
        if (arg == "--table") { opts.outputFormat = 3; continue; }
        if (arg == "--pipe" && i + 1 < argc) { opts.pipeCommand = argv[++i]; continue; }
        if (arg == "--help" || arg == "-h" || arg == "/?" || arg == "-?") {
            printUsage(argv[0]);
            std::exit(0);
        }
        if (arg == "--version" || arg == "-V") {
            printVersion();
            std::exit(0);
        }
        if (arg == "--") {
            for (int j = i + 1; j < argc; ++j) {
                if (!opts.forceSystemCrontab) opts.crontabPath = argv[j];
            }
            break;
        }
        if (arg == "--system-crontab") {
            opts.forceSystemCrontab = true;
            continue;
        }

        if (opts.forceSystemCrontab) {
            std::cerr << "cron: custom crontab path is not allowed with --system-crontab. Required path: C:\\cron\\crontab.txt\n";
            return false;
        }

        opts.crontabPath = arg;
    }

    if (opts.forceSystemCrontab) {
        opts.crontabPath = "C:\\cron\\crontab.txt";
    }

    return true;
}
