#include "options.hpp"

void NfsctlHelpSystem::PrintMainHelp() {
    wprintf(LR"(nfsctl(1)               CrossShell for UNIX Reference Manual                 nfsctl(1)

    NAME
        nfsctl - NFS client, mounts, exports, and RPC diagnostic control utility

    SYNOPSIS
        nfsctl TOOL_SWITCH [OPTIONS] [ARGUMENTS...]

    DESCRIPTION
        Manages Windows Client for NFS, active NFS network mounts, remote server
        exports, and RPC diagnostic configurations. Integrates with the Windows
        Networking API (WNet), Client for NFS Registry settings, and RPC portmapper
        subsystems to provide unified NFS administration.

    TOOL SWITCHES
        -m, --mounts
            Inspect active network and NFS drive mounts.

        --mount PATH DRIVE:
            Mount an NFS share (e.g. \\192.168.1.50\srv Z:).

        --umount DRIVE:
            Unmount a mapped network drive (e.g. Z:).

        -e, --exportfs
            Enumerate local Windows Server for NFS exports.

        -s, --showmount HOST
            Probe a remote NFS server for exports and mount points (NFSv3/NFSv4).

        -st, --nfsstat
            Query Windows Client for NFS service and Registry state.

        -c, --nfsconf
            Read or set Windows Client for NFS Registry configuration.

        -id, --nfsidmap USER
            Resolve Windows User Account to SID mapping.

        -d, --rpcdebug
            Read or set Client for NFS Registry DebugFlags.

    GLOBAL OPTIONS
        --output FORMAT
            Select table, csv, tsv, or json output. The default is table.

        --json, -j, --csv, --tsv, --table
            Convenience shortcuts for structured output formats.

        --pipe COMMAND
            Stream formatted output directly to another command or utility.

        --dry-run
            Preview write operations without applying registry changes.

        --confirm
            Confirm write operations that modify state.

        --force
            Allow overriding existing differing registry values.

        --backup-reg [PATH]
            Save pre-change registry values to a backup file.

        --timeout-ms MS
            Network probe timeout in milliseconds (default 1500).

        --ipv4, --ipv6, --dual-stack
            Select socket probe IP protocol family.

        -q, --quiet
            Suppress informational diagnostics.

        --verbose, --trace
            Display detailed execution diagnostics or trace-level logging.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version and license information.

    EXAMPLES
        nfsctl --mounts
            List all active NFS and network drive mounts.

        nfsctl --showmount 192.168.1.50
            Probe exports on a remote NFS server.

        nfsctl --mount \\192.168.1.50\exports Z:
            Mount an NFS share to drive letter Z:.

        nfsctl --umount Z:
            Unmount network drive Z:.

        nfsctl --nfsstat --json
            Query Client for NFS status and emit JSON.

        nfsctl --showmount 192.168.1.50 --csv
            Export remote share list as CSV.

    CrossShell for UNIX                                                     nfsctl(1)
)");
}

bool NfsctlHelpSystem::HasHelpFlag(const std::vector<std::wstring>& args) {
    for (const auto& arg : args) {
        if (arg == L"--help" || arg == L"-h") {
            return true;
        }
    }
    return false;
}

DWORD NfsctlOptionsParser::ParseGlobalOptions(std::vector<std::wstring>& args, GlobalOptions& options) {
    std::vector<std::wstring> filtered;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::wstring& arg = args[i];
        if (arg == L"--json" || arg == L"-j") {
            options.outputMode = OutputMode::Json;
            continue;
        }
        if (arg == L"--csv") {
            options.outputMode = OutputMode::Csv;
            continue;
        }
        if (arg == L"--tsv") {
            options.outputMode = OutputMode::Tsv;
            continue;
        }
        if (arg == L"--dry-run") {
            options.dryRun = true;
            continue;
        }
        if (arg == L"--confirm") {
            options.confirm = true;
            continue;
        }
        if (arg == L"--force") {
            options.forceWrite = true;
            continue;
        }
        if (arg == L"--backup-reg") {
            options.backupReg = true;
            if (i + 1 < args.size() && !args[i + 1].empty() && args[i + 1][0] != L'-') {
                options.backupRegPath = args[++i];
            }
            continue;
        }
        if (arg == L"--win32-exit") {
            options.win32Exit = true;
            continue;
        }
        if (arg == L"--quiet" || arg == L"-q") {
            options.quiet = true;
            continue;
        }
        if (arg == L"--verbose") {
            options.verbose = true;
            continue;
        }
        if (arg == L"--trace") {
            options.trace = true;
            options.verbose = true;
            continue;
        }
        if (arg == L"--ipv4") {
            options.addressFamily = AF_INET;
            continue;
        }
        if (arg == L"--ipv6") {
            options.addressFamily = AF_INET6;
            continue;
        }
        if (arg == L"--dual-stack") {
            options.addressFamily = AF_UNSPEC;
            continue;
        }
        if (arg == L"--nfsv3") {
            options.nfsProbeMode = NfsProbeMode::V3;
            continue;
        }
        if (arg == L"--nfsv4") {
            options.nfsProbeMode = NfsProbeMode::V4;
            continue;
        }
        if (arg == L"--nfsboth") {
            options.nfsProbeMode = NfsProbeMode::Both;
            continue;
        }
        if (arg == L"--timeout-ms") {
            if (i + 1 >= args.size()) {
                fwprintf(stderr, L"Error: --timeout-ms requires a value.\n");
                return ERROR_INVALID_PARAMETER;
            }
            LONG timeout = 0;
            if (!NfsSecurityManager::SafeWstol(args[++i].c_str(), &timeout) || timeout <= 0) {
                fwprintf(stderr, L"Error: --timeout-ms expects a positive integer.\n");
                return ERROR_INVALID_PARAMETER;
            }
            options.timeoutMs = static_cast<int>(timeout);
            continue;
        }
        if (arg == L"--op-timeout-ms") {
            if (i + 1 >= args.size()) {
                fwprintf(stderr, L"Error: --op-timeout-ms requires a value.\n");
                return ERROR_INVALID_PARAMETER;
            }
            LONG timeout = 0;
            if (!NfsSecurityManager::SafeWstol(args[++i].c_str(), &timeout) || timeout < 0) {
                fwprintf(stderr, L"Error: --op-timeout-ms expects a non-negative integer.\n");
                return ERROR_INVALID_PARAMETER;
            }
            options.opTimeoutMs = static_cast<int>(timeout);
            continue;
        }
        filtered.push_back(arg);
    }

    if (options.trace) {
        options.verbose = true;
    }

    args.swap(filtered);
    return ERROR_SUCCESS;
}
