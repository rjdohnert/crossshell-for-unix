#include "sar_options.hpp"

void SarOptionParser::printHelp() {
    std::cout <<
R"(sar(1)                   CrossShell for UNIX Reference Manual                    sar(1)

NAME
    sar - system activity reporter

SYNOPSIS
    sar [OPTIONS] [INTERVAL [COUNT]]

DESCRIPTION
    Reports comprehensive system activity statistics including CPU
    utilization, memory and swap usage, disk I/O, network interface
    statistics, paging activity, and process run-queue information.

OPTIONS
    -u
        Report CPU utilization (%usr, %sys, %wio, %idle). [Default]
    -r
        Report memory and swap utilization (freemem, freeswp, %memused).
    -d
        Report physical disk aggregate activity (%busy, avque, r+w/s, blks/s, avwait).
    -D
        Report per-device physical disk activity, one row per disk instance.
    -v
        Report kernel table activity (processes, threads, handles).
    -M
        Report per-core CPU utilization (%usr, %sys, %idle).
    -b
        Report block-device transfer statistics (HP-UX sar -b style).
    -n
        Report network interface activity (rxpck/s, txpck/s, rxkB/s, txkB/s).
    -q
        Report process run-queue length and active task stats.
    -w
        Report paging activity (pages in/out per second).
    --csv
        Emit CSV rows for script-friendly output.
    --json
        Emit JSON rows for script-friendly output.
    -A
        Report all supported sar-compatible system activity statistics.
    -h, --help, /?
        Display this comprehensive reference manual and exit.

EXAMPLES
    sar 1 5
        Sample CPU utilization every 1 second, 5 times.

    sar -r 2 10
        Sample memory statistics every 2 seconds, 10 times.

    sar -d 1 0
        Continuously sample disk I/O every 1 second.

    sar -M --csv 1 3
        Report per-core CPU in CSV format.

    sar -A 1 3
        Report complete sar-compatible diagnostics 3 times.

EXIT STATUS
    0   Success.
    1   Invalid options or sampling failure.

    CrossShell for UNIX                                                   sar(1)
)";
}

bool SarOptionParser::parse(int argc, char* argv[], SarOptions& opts) {
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "-help" || arg == "/?" || arg == "--help") {
            printHelp();
            std::exit(0);
        } else if (arg == "-u") opts.cpu = true;
        else if (arg == "-r") opts.memory = true;
        else if (arg == "-d") opts.disk = true;
        else if (arg == "-D") opts.diskDetail = true;
        else if (arg == "-v") opts.kernel = true;
        else if (arg == "-M") opts.perCore = true;
        else if (arg == "-b") opts.io = true;
        else if (arg == "-n") opts.network = true;
        else if (arg == "-q") opts.queue = true;
        else if (arg == "-w") opts.page = true;
        else if (arg == "--csv") opts.csv = true;
        else if (arg == "--json") opts.json = true;
        else if (arg == "-A") opts.all = true;
        else if (arg[0] != '-') positional.push_back(arg);
    }

    if (opts.swap) opts.page = true;
    if (opts.page) opts.swap = true;
    if (opts.io) opts.disk = true;
    if (opts.diskDetail) opts.disk = true;

    if (opts.all) {
        opts.cpu = opts.memory = opts.disk = opts.diskDetail = opts.page = opts.kernel = opts.perCore = opts.io = opts.network = opts.queue = opts.swap = true;
    } else if (!opts.cpu && !opts.memory && !opts.disk && !opts.diskDetail && !opts.page && !opts.kernel && !opts.perCore && !opts.io && !opts.network && !opts.queue && !opts.swap) {
        opts.cpu = true; // Default behavior when no flag specified
    }

    if (!positional.empty()) {
        try {
            opts.interval = std::stoi(positional[0]);
            if (positional.size() > 1) opts.count = std::stoi(positional[1]);
            else opts.count = 0; // Infinite loop if count omitted
        } catch (...) {
            std::cerr << "sar: invalid numeric interval or count\n";
            return false;
        }
    }

    return true;
}
