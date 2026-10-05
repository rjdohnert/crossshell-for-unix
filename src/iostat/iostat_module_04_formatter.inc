// ============================================================================
// 4. OUTPUT FORMATTER
// ============================================================================

class OutputFormatter {
public:
    static void PrintUsage() {
        std::cout << R"(iostat(1)          CrossShell for UNIX Reference Manual                iostat(1)

    NAME
        iostat - report CPU and physical disk I/O statistics

    SYNOPSIS
        iostat [OPTIONS] [INTERVAL] [COUNT]

    DESCRIPTION
        iostat queries Windows PDH performance counters to report CPU utilization
        percentages and physical disk throughput and queue metrics. If no interval
        is specified, a single snapshot report is generated.

    OPTIONS
        -c, --count COUNT
            Number of reports to generate before exiting.

        -n, --interval SECONDS
            Delay in seconds between successive reports.

        -k, --kilobytes
            Display disk throughput in kilobytes per second (default).

        -m, --megabytes
            Display disk throughput in megabytes per second.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        iostat
            Display single snapshot of CPU and disk activity.

        iostat 2 5
            Report statistics every 2 seconds for 5 iterations.

        iostat -m 1 10
            Display throughput in MB/s every second 10 times.

    CrossShell for UNIX                                                   iostat(1)
)";
    }

    static void PrintVersion() {
        std::cout << "iostat v1.0.0\n";
    }

    static void PrintCpuMetrics(double userPct, double sysPct, double idlePct) {
        std::cout << "\navg-cpu:  %user   %sys   %idle\n";
        std::cout << "          " 
                  << std::setw(5) << std::fixed << std::setprecision(2) << userPct << "  "
                  << std::setw(5) << sysPct << "  "
                  << std::setw(5) << idlePct << "\n\n";
    }

    static void PrintDiskTable(const std::map<std::wstring, DiskStats>& diskMap, double scale) {
        std::vector<std::pair<std::wstring, DiskStats>> individual_disks;
        std::pair<std::wstring, DiskStats> total_disk;
        bool has_total = false;

        for (auto const& pair : diskMap) {
            if (pair.first == L"_Total") {
                total_disk = pair;
                has_total = true;
            } else {
                individual_disks.push_back(pair);
            }
        }

        std::cout << std::left << std::setw(22) << "Device"
                  << std::right << std::setw(10) << "tps"
                  << std::setw(14) << "kB_read/s"
                  << std::setw(14) << "kB_wrtn/s"
                  << std::setw(10) << "aqu-sz"
                  << std::setw(10) << "%util" << "\n";

        auto printRow = [&](const std::wstring& wname, const DiskStats& stats) {
            std::string name = StringUtils::WideToNarrow(wname);
            double tps = stats.reads_sec + stats.writes_sec;
            double throughput_read = stats.read_bytes_sec / scale;
            double throughput_write = stats.write_bytes_sec / scale;
            double aqu_sz = stats.queue_length;
            double util = stats.pct_disk_time;

            std::cout << std::left << std::setw(22) << name
                      << std::right << std::setw(10) << std::fixed << std::setprecision(2) << tps
                      << std::setw(14) << throughput_read
                      << std::setw(14) << throughput_write
                      << std::setw(10) << aqu_sz
                      << std::setw(10) << util << "\n";
        };

        for (const auto& disk : individual_disks) {
            printRow(disk.first, disk.second);
        }

        if (has_total) {
            std::cout << std::string(80, '-') << "\n";
            printRow(total_disk.first, total_disk.second);
        }
    }
};

