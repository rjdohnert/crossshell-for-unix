#include "format_helper.hpp"
#include "process_snapshot.hpp"
#include "process_usage_inspector.hpp"
#include "ulimit_reporter.hpp"

void UlimitReporter::PrintSummary(OutputFormat format) {
        ProcessSnapshot snapshot = {};
        if (!ProcessUsageInspector::SnapshotProcess(GetCurrentProcess(), snapshot)) {
            std::wcerr << L"ulimit: unable to query current process resource usage\n";
            return;
        }

        if (format == OutputFormat::Json) {
            std::wcout << L"{\"working_set\":" << snapshot.working_set 
                       << L",\"peak_working_set\":" << snapshot.peak_working_set 
                       << L",\"private_usage\":" << snapshot.private_usage 
                       << L",\"user_time_100ns\":" << snapshot.user_time_100ns 
                       << L",\"kernel_time_100ns\":" << snapshot.kernel_time_100ns
                       << L",\"open_handles\":" << snapshot.handle_count << L"}\n";
            return;
        }
        if (format == OutputFormat::Csv) {
            std::wcout << L"metric,value,formatted\n" 
                       << L"working_set," << snapshot.working_set << L"," << FormatHelper::FormatBytesIec(snapshot.working_set) << L"\n"
                       << L"peak_working_set," << snapshot.peak_working_set << L"," << FormatHelper::FormatBytesIec(snapshot.peak_working_set) << L"\n"
                       << L"private_usage," << snapshot.private_usage << L"," << FormatHelper::FormatBytesIec(snapshot.private_usage) << L"\n"
                       << L"user_time_100ns," << snapshot.user_time_100ns << L"," << FormatHelper::FormatDurationSeconds(snapshot.user_time_100ns) << L"\n"
                       << L"kernel_time_100ns," << snapshot.kernel_time_100ns << L"," << FormatHelper::FormatDurationSeconds(snapshot.kernel_time_100ns) << L"\n"
                       << L"open_handles," << snapshot.handle_count << L"," << snapshot.handle_count << L"\n";
            return;
        }
        if (format == OutputFormat::Tsv) {
            std::wcout << L"METRIC\tVALUE\tFORMATTED\n" 
                       << L"working_set\t" << snapshot.working_set << L"\t" << FormatHelper::FormatBytesIec(snapshot.working_set) << L"\n"
                       << L"peak_working_set\t" << snapshot.peak_working_set << L"\t" << FormatHelper::FormatBytesIec(snapshot.peak_working_set) << L"\n"
                       << L"private_usage\t" << snapshot.private_usage << L"\t" << FormatHelper::FormatBytesIec(snapshot.private_usage) << L"\n"
                       << L"user_time_100ns\t" << snapshot.user_time_100ns << L"\t" << FormatHelper::FormatDurationSeconds(snapshot.user_time_100ns) << L"\n"
                       << L"kernel_time_100ns\t" << snapshot.kernel_time_100ns << L"\t" << FormatHelper::FormatDurationSeconds(snapshot.kernel_time_100ns) << L"\n"
                       << L"open_handles\t" << snapshot.handle_count << L"\t" << snapshot.handle_count << L"\n";
            return;
        }
        if (format == OutputFormat::Table) {
            std::wcout << L"RESOURCE                       CURRENT VALUE          LIMIT\n"
                       << L"-----------------------------------------------------------\n"
                       << L"core file size (blocks, -c)    unlimited              unlimited\n"
                       << L"data seg size (kbytes, -d)     unlimited              unlimited\n"
                       << L"scheduling priority (-e)       0                      0\n"
                       << L"file size (blocks, -f)         unlimited              unlimited\n"
                       << L"pending signals (-i)           unlimited              unlimited\n"
                       << L"max locked memory (kbytes, -l) unlimited              unlimited\n"
                       << L"max memory size (MB, -m)       " << FormatHelper::FormatBytesIec(snapshot.private_usage) << L"              unlimited\n"
                       << L"open files / handles (-n)      " << snapshot.handle_count << L"                    unlimited\n"
                       << L"pipe size (512 bytes, -p)      8                      8\n"
                       << L"POSIX message queues (-q)      unlimited              unlimited\n"
                       << L"real-time priority (-r)        0                      0\n"
                       << L"stack size (kbytes, -s)        1024                   unlimited\n"
                       << L"cpu time (seconds, -t)         " << FormatHelper::FormatDurationSeconds(snapshot.user_time_100ns) << L"               unlimited\n"
                       << L"max user processes (-u)        unlimited              unlimited\n"
                       << L"virtual memory (MB, -v)        " << FormatHelper::FormatBytesIec(snapshot.working_set) << L"              unlimited\n"
                       << L"file locks (-x)                unlimited              unlimited\n";
            return;
        }

        std::wcout << L"core file size          (blocks, -c) unlimited\n"
                   << L"data seg size           (kbytes, -d) unlimited\n"
                   << L"scheduling priority             (-e) 0\n"
                   << L"file size               (blocks, -f) unlimited\n"
                   << L"pending signals                 (-i) unlimited\n"
                   << L"max locked memory       (kbytes, -l) unlimited\n"
                   << L"max memory size                 (MB, -m) " << FormatHelper::FormatBytesIec(snapshot.private_usage) << L"\n"
                   << L"open files                      (-n) " << snapshot.handle_count << L"\n"
                   << L"pipe size            (512 bytes, -p) 8\n"
                   << L"POSIX message queues     (bytes, -q) unlimited\n"
                   << L"real-time priority              (-r) 0\n"
                   << L"stack size              (kbytes, -s) 1024\n"
                   << L"cpu time               (seconds, -t) " << FormatHelper::FormatDurationSeconds(snapshot.user_time_100ns) << L"\n"
                   << L"max user processes              (-u) unlimited\n"
                   << L"virtual memory                  (MB, -v) " << FormatHelper::FormatBytesIec(snapshot.working_set) << L"\n"
                   << L"file locks                      (-x) unlimited\n";
    }

void UlimitReporter::PrintHelp() {
        std::wcout << LR"(ulimit(1)               CrossShell for UNIX Reference Manual                 ulimit(1)

    NAME
        ulimit - get and set process and job resource limits

    SYNOPSIS
        ulimit [-SHacdflmnpqrstuvx] [LIMIT]
        ulimit [QUALIFIERS] [--] [COMMAND [ARGUMENTS...]]
        ulimit [OPTIONS]

    DESCRIPTION
        ulimit provides control over the resources available to the current
        shell, child processes, or command execution contexts. On Windows,
        resource constraints are enforced via Windows NT Job Objects and
        Process Token Security Privilege management.

    RESOURCE LIMIT QUALIFIERS
        -a, --all
            Report all current process resource usages and supported limits.

        -c [LIMIT]
            Core file size limit (Windows Crash Dump limit).

        -d [LIMIT]
            Maximum data segment size (kbytes).

        -e [LIMIT]
            Maximum scheduling priority (nice level).

        -f [LIMIT]
            Maximum file size written (blocks).

        -l [LIMIT]
            Maximum locked-in-memory address space (kbytes).

        -m, --memory [LIMIT_MB]
            Maximum physical memory / working set size (MB).

        -n [LIMIT]
            Maximum number of open file handles / descriptors.

        -p [LIMIT]
            Pipe buffer size in 512-byte blocks.

        -s [LIMIT]
            Maximum stack size (kbytes).

        -t, --cpu [SECONDS]
            Maximum CPU user time allowed in seconds.

        -u, --processes [N]
            Maximum number of active processes in the job.

        -v, --virtual-memory [LIMIT_MB]
            Maximum virtual memory / pagefile commit limit (MB).

        -w, --working-set [LIMIT_MB]
            Maximum working set size (MB).

        -x [LIMIT]
            Maximum number of file locks.

        -S, --soft
            Set or query the soft resource limit.

        -H, --hard
            Set or query the hard resource limit.

    WINDOWS JOB CONTROLS
        --priority LEVEL
            Set process priority class: idle, below_normal, normal,
            above_normal, high, or realtime.

        --kill-on-close
            Ensure all child processes terminate when the job handle closes.

    OUTPUT & INTEGRATION OPTIONS
        --output FORMAT
            Select table, csv, tsv, or json output. The default is table.

        --json, -j, --csv, --tsv, --table
            Convenience shortcuts for structured output formats.

        --pipe COMMAND
            Stream formatted output directly to another command or utility.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

        --
            Delimit options from command and arguments to execute.

    EXAMPLES
        ulimit -a
            Display current resource usage snapshot and limits.

        ulimit -a --json
            Display resource snapshot formatted as JSON.

        ulimit -m 512 -- myapp.exe
            Run myapp.exe constrained to 512 MB memory.

        ulimit -t 30 -m 1024 -- cmd.exe /c heavy_task.cmd
            Execute task with a 30-second CPU limit and 1 GB memory limit.

        ulimit -u 4 -- make -j8
            Restrict process tree to at most 4 active concurrent processes.

    CrossShell for UNIX                                                     ulimit(1)
)";
    }

void UlimitReporter::PrintVersion() {
        std::wcout << L"ulimit (CrossShell) 5.0.0\n"
                   << L"Copyright (c) 2026 Roberto J Dohnert. All rights reserved.\n";
    }
